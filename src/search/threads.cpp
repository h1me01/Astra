#include <algorithm>
#include <unordered_map>

#include "threads.h"
#include "types.h"

namespace astra::search {

void ThreadPool::set_count(int count) {
    stop();
    wait();
    terminate();

    started_threads_ = 0;

    threads_.reserve(count);
    running_threads_.reserve(count);

    for (int i = 0; i < count; ++i) {
        threads_.emplace_back(std::make_unique<Search>());
        running_threads_.emplace_back(std::make_unique<std::thread>(&Search::idle, threads_[i].get()));
    }

    // give threads a moment to start up
    while (started_threads_ < static_cast<size_t>(count)) {
    }
}

void ThreadPool::wait(bool include_main) {
    for (size_t i = (include_main ? 0 : 1); i < threads_.size(); ++i) {
        std::unique_lock search_lock(threads_[i]->mutex);
        threads_[i]->cv.wait(search_lock, [this, i] { return !threads_[i]->searching; });
    }
}

void ThreadPool::terminate() {
    for (auto& th : threads_) {
        std::lock_guard lock(th->mutex);
        th->exiting = true;
        th->searching = true;
        th->cv.notify_all();
    }

    // wait for all threads to finish
    for (auto& running_thread : running_threads_)
        if (running_thread && running_thread->joinable())
            running_thread->join();

    threads_.clear();
    running_threads_.clear();
}

void ThreadPool::launch_workers(const Board& board, Limits limits) {
    stop();
    wait();

    stop_ = false;

    for (auto& th : threads_) {
        std::lock_guard lock(th->mutex);
        th->board = board;
        th->limits = limits;
        th->searching = true;
        th.get()->cv.notify_all();
    }
}

Search* ThreadPool::pick_best() {
    Search* best_thread = main_thread();

    if (threads_.size() == 1 || best_thread->limits.multipv != 1)
        return best_thread;

    std::unordered_map<uint16_t, Score> votes;
    Score min_score = SCORE_INFINITE;

    for (auto& th : threads_) {
        if (!th->completed_depth())
            continue;
        min_score = std::min(min_score, th->best_root_move().score);
    }

    for (auto& th : threads_) {
        const int root_depth = th->completed_depth();
        if (!root_depth)
            continue;

        const auto& rm = th->best_root_move();
        votes[rm.raw()] += (rm.score - min_score + 10) * root_depth;
    }

    for (auto& th : threads_) {
        Search* thread = th.get();

        const auto& rm = thread->best_root_move();
        if (!thread->completed_depth())
            continue;

        const auto& best_rm = best_thread->best_root_move();

        if (is_decisive(best_rm.score)) {
            if (rm.score > best_rm.score)
                best_thread = thread;
        } else if (is_win(rm.score)) {
            best_thread = thread;
        } else if (!is_loss(rm.score) && votes[rm.raw()] > votes[best_rm.raw()]) {
            best_thread = thread;
        }
    }

    return best_thread;
}

// Global Variable

ThreadPool thread_pool;

} // namespace astra::search
