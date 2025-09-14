#include <unordered_map>

#include "threads.h"

namespace Engine {

ThreadPool::~ThreadPool() {
    // signal all threads to exit
    for(auto &th : threads) {
        th->exiting = true;
        launch_worker(th.get());
    }

    // wait for all threads to finish
    for(auto &running_thread : running_threads)
        if(running_thread && running_thread->joinable())
            running_thread->join();

    threads.clear();
    running_threads.clear();
}

void ThreadPool::launch_worker(Search *worker) {
    std::lock_guard<std::mutex> lock(worker->mutex);
    worker->searching = true;
    worker->cv.notify_all();
}

void ThreadPool::launch_workers(const Board &board, Limits limits) {
    stop_flag.store(false, std::memory_order_release);

    for(auto &th : threads) {
        th->board = board;
        th->limits = limits;
        launch_worker(th.get());
    }
}

void ThreadPool::set_count(int count) {
    wait();

    // stop and clean up existing threads
    for(int i = 0; i < (int) threads.size(); i++) {
        auto &th = threads[i];
        th->exiting = true;
        launch_worker(th.get());

        if(running_threads[i] && running_threads[i]->joinable())
            running_threads[i]->join();
    }

    // clear old threads
    threads.clear();
    running_threads.clear();

    started_threads.store(0, std::memory_order_release);

    // create new threads
    threads.reserve(count);
    running_threads.reserve(count);

    for(int i = 0; i < count; i++) {
        threads.emplace_back(std::make_unique<Search>());
        running_threads.emplace_back(std::make_unique<std::thread>(&Search::idle, threads[i].get()));
    }

    // give threads a moment to start up
    while(started_threads.load(std::memory_order_acquire) < (size_t) count) {
    }
}

void ThreadPool::wait(bool include_main_thread) {
    // copy the thread pointers while holding the mutex
    std::vector<Search *> thread_ptrs;
    {
        if(threads.empty())
            return;

        size_t start_idx = include_main_thread ? 0 : 1;
        size_t count = threads.size() - start_idx;

        if(count > 0) {
            thread_ptrs.reserve(count);
            for(size_t i = start_idx; i < threads.size(); i++)
                thread_ptrs.push_back(threads[i].get());
        }
    }

    // wait for each thread without holding threads_mutex
    for(auto *th : thread_ptrs) {
        std::unique_lock search_lock(th->mutex);
        th->cv.wait(search_lock, [th] { return !th->searching; });
    }
}

Search *ThreadPool::pick_best_thread() {
    Search *best_thread = main_thread();

    if(get_count() == 1 || best_thread->limits.multipv != 1)
        return best_thread;

    std::unordered_map<uint16_t, int> votes;
    Score min_score = VALUE_INFINITE;

    for(auto &th : threads) {
        const auto &rm = th->root_moves[0];
        if(!rm.depth)
            continue;
        min_score = std::min(min_score, Score(rm.get_score()));
    }

    for(auto &th : threads) {
        const int root_depth = th->root_moves[0].depth;
        if(!root_depth)
            continue;

        const auto &rm = th->root_moves[0];
        votes[rm.raw()] += (rm.get_score() - min_score + 10) * root_depth;
    }

    for(auto &th : threads) {
        Search *thread = th.get();

        const RootMove &rm = thread->root_moves[0];
        if(!rm.depth)
            continue;

        const RootMove &best_rm = best_thread->root_moves[0];

        if(is_decisive(best_rm.get_score())) {
            if(rm.get_score() > best_rm.get_score())
                best_thread = thread;
        } else if(is_win(rm.get_score()))
            best_thread = thread;
        else if(!is_loss(rm.get_score()) && votes[rm.raw()] > votes[best_rm.raw()])
            best_thread = thread;
    }

    return best_thread;
}

ThreadPool threads;

} // namespace Engine
