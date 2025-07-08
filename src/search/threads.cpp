#include "threads.h"

namespace Astra {

void ThreadPool::launch_workers(const Board &board, Limits limit, int worker_count, bool use_tb) {
    start();

    threads.clear();
    running_threads.clear();

    for(int i = 0; i < worker_count; i++) {
        auto thread = std::make_unique<Search>(STARTING_FEN);
        thread->id = i;
        thread->board = board;
        thread->limit = limit;
        thread->use_tb = use_tb;

        running_threads.emplace_back(&Search::bestmove, thread.get());
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

U64 ThreadPool::get_nodes() const {
    U64 total_nodes = 0;
    for(const auto &t : threads)
        total_nodes += t->get_nodes();
    return total_nodes;
}

U64 ThreadPool::get_tb_hits() const {
    U64 total_tb_hits = 0;
    for(const auto &t : threads)
        total_tb_hits += t->get_tb_hits();
    return total_tb_hits;
}

// global variable
ThreadPool threads;

} // namespace Astra
