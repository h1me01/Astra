#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "search.h"

using namespace chess;

namespace search {

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

    int size() const {
        return static_cast<int>(threads.size());
    }

    U64 total_nodes() const {
        U64 count = 0;
        for(const auto &t : threads)
            count += t->get_nodes();
        return count;
    }

    U64 tb_hits() const {
        U64 count = 0;
        for(const auto &t : threads)
            count += t->get_tb_hits();
        return count;
    }

  private:
    std::atomic<bool> stop_flag;
    std::atomic<size_t> started_threads;

    std::vector<std::unique_ptr<Search>> threads;
    std::vector<std::unique_ptr<std::thread>> running_threads;
};

// global variable

extern ThreadPool threads;

} // namespace search
