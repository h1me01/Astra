#include "threads.h"

namespace Search {

void ThreadPool::launch_workers(const Board &board, Limits limits, int worker_count) {
    start();

    threads.clear();
    running_threads.clear();

    for(int i = 0; i < worker_count; i++) {
        auto thread = std::make_unique<Search>();
        thread->id = i;

        running_threads.emplace_back(
            [thread_ptr = thread.get(), &board, limits]() mutable { thread_ptr->start(board, limits); });

        threads.emplace_back(std::move(thread));
    }
}

void ThreadPool::force_stop() {
    stop();

    // wait for all worker threads to finish
    for(auto &th : running_threads)
        if(th.joinable())
            th.join();

    threads.clear();
    running_threads.clear();
}

bool ThreadPool::is_stopped() const {
    return stop_flag.load(std::memory_order_relaxed);
}

U64 ThreadPool::get_total_nodes() const {
    U64 total_nodes = 0;
    for(const auto &t : threads)
        total_nodes += t->get_total_nodes();
    return total_nodes;
}

// global variable
ThreadPool threads;

} // namespace Search
