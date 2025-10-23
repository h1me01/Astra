#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "search.h"

using namespace Chess;

namespace Engine {

class ThreadPool {
  public:
    ThreadPool() : stop_flag(false), started_threads(0) {}

    ~ThreadPool() {
        terminate();
    }

    void set_count(int count);
    void wait(bool include_main = true);

    void terminate();
    void launch_workers(const Board &board, Limits limit);

    Search *pick_best();

    void add_started_thread() {
        started_threads++;
    }

    void new_game() {
        for(auto &th : threads)
            th->clear_histories();
    }

    void stop() {
        stop_flag = true;
    }

    bool is_stopped() const {
        return stop_flag.load(std::memory_order_relaxed);
    }

    Search *main_thread() {
        return threads.empty() ? nullptr : threads[0].get();
    }

    U64 get_total_nodes() const {
        U64 total_nodes = 0;
        for(const auto &t : threads)
            total_nodes += t->get_nodes();
        return total_nodes;
    }

    U64 get_tb_hits() const {
        U64 total_tb_hits = 0;
        for(const auto &t : threads)
            total_tb_hits += t->get_tb_hits();
        return total_tb_hits;
    }

  private:
    std::atomic<bool> stop_flag;
    std::atomic<size_t> started_threads;

    std::vector<std::unique_ptr<Search>> threads;
    std::vector<std::unique_ptr<std::thread>> running_threads;
};

// global variable

extern ThreadPool threads;

} // namespace Engine
