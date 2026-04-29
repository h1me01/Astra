#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "search.h"

namespace astra::search {

class ThreadPool {
  public:
    ThreadPool()
        : stop_(false),
          started_threads_(0) {}

    ~ThreadPool() { terminate(); }

    void set_count(int count);
    void wait(bool include_main = true);
    void terminate();
    void launch_workers(const Board& board, Limits limit);
    Search* pick_best();
    void add_started_thread() { started_threads_++; }

    void new_game() {
        for (auto& th : threads_)
            th->clear_histories();
    }

    void stop() { stop_ = true; }
    bool is_stopped() const { return stop_.load(std::memory_order_relaxed); }
    Search* main_thread() { return threads_.empty() ? nullptr : threads_[0].get(); }
    int size() const { return static_cast<int>(threads_.size()); }

    uint64_t total_nodes() const {
        uint64_t count = 0;
        for (const auto& t : threads_)
            count += t->nodes();
        return count;
    }

    uint64_t tb_hits() const {
        uint64_t count = 0;
        for (const auto& t : threads_)
            count += t->tb_hits();
        return count;
    }

  private:
    std::atomic<bool> stop_;
    std::atomic<size_t> started_threads_;
    std::vector<std::unique_ptr<Search>> threads_;
    std::vector<std::unique_ptr<std::thread>> running_threads_;
};

// Global Variable

extern ThreadPool thread_pool;

} // namespace astra::search
