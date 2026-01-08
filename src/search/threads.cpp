#include <unordered_map>

#include "threads.h"

namespace search {

void ThreadPool::set_count(int count) {
    stop();
    wait();
    terminate();

    started_threads = 0;

    // create new threads
    threads.reserve(count);
    running_threads.reserve(count);

    for(int i = 0; i < count; i++) {
        threads.emplace_back(std::make_unique<Search>());
        running_threads.emplace_back(std::make_unique<std::thread>(&Search::idle, threads[i].get()));
    }

    // give threads a moment to start up
    while(started_threads < static_cast<size_t>(count)) {
    }
}

void ThreadPool::wait(bool include_main) {
    for(size_t i = (include_main ? 0 : 1); i < threads.size(); i++) {
        std::unique_lock search_lock(threads[i]->mutex);
        threads[i]->cv.wait(search_lock, [this, i] { return !threads[i]->searching; });
    }
}

void ThreadPool::terminate() {
    // signal all threads to exit
    for(auto &th : threads) {
        std::lock_guard lock(th->mutex);
        th->exiting = true;
        th->searching = true;
        th->cv.notify_all();
    }

    // wait for all threads to finish
    for(auto &running_thread : running_threads)
        if(running_thread && running_thread->joinable())
            running_thread->join();

    threads.clear();
    running_threads.clear();
}

void ThreadPool::launch_workers(const Board &board, Limits limits) {
    stop();
    wait();

    stop_flag = false;

    for(auto &th : threads) {
        std::lock_guard lock(th->mutex);
        th->board = board;
        th->limits = limits;
        th->searching = true;
        th.get()->cv.notify_all();
    }
}

Search *ThreadPool::pick_best() {
    Search *best_thread = main_thread();

    if(threads.size() == 1 || best_thread->limits.multipv != 1)
        return best_thread;

    std::unordered_map<uint16_t, int> votes;
    Score min_score = SCORE_INFINITE;

    for(auto &th : threads) {
        if(!th->get_completed_depth())
            continue;
        min_score = std::min(min_score, Score(th->get_best_move().score));
    }

    for(auto &th : threads) {
        const int root_depth = th->get_completed_depth();
        if(!root_depth)
            continue;

        const auto &rm = th->get_best_move();
        votes[rm.raw()] += (rm.score - min_score + 10) * root_depth;
    }

    for(auto &th : threads) {
        Search *thread = th.get();

        const auto &rm = thread->get_best_move();
        if(!thread->get_completed_depth())
            continue;

        const auto &best_rm = best_thread->get_best_move();

        if(is_decisive(best_rm.score)) {
            if(rm.score > best_rm.score)
                best_thread = thread;
        } else if(is_win(rm.score)) {
            best_thread = thread;
        } else if(!is_loss(rm.score) && votes[rm.raw()] > votes[best_rm.raw()]) {
            best_thread = thread;
        }
    }

    return best_thread;
}

// global variable

ThreadPool threads;

} // namespace search
