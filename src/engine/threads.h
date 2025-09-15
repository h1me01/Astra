#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

#include "search.h"

using namespace Chess;

namespace Engine {

class ThreadPool {
  public:
    ThreadPool() : started_threads(0) {}
    ~ThreadPool();

    void launch_workers(const Board &board, Limits limit);

    void set_count(int count);

    void wait(bool include_main_thread = true);

    Search *pick_best_thread();

    void add_started_thread() {
        started_threads.fetch_add(1, std::memory_order_acq_rel);
    }

    void new_game() {
        for(auto &th : threads)
            th->clear_histories();
    }

    void stop() {
        stop_flag.store(true, std::memory_order_release);
    }

    Search *main_thread() {
        return threads.empty() ? nullptr : threads[0].get();
    }

    bool is_stopped() const {
        return stop_flag.load(std::memory_order_acquire);
    }

    U64 get_total_nodes() const {
        U64 total_nodes = 0;
        for(const auto &t : threads)
            total_nodes += t->get_total_nodes();
        return total_nodes;
    }

    U64 get_tb_hits() const {
        U64 total_tb_hits = 0;
        for(const auto &t : threads)
            total_tb_hits += t->get_tb_hits();
        return total_tb_hits;
    }

  private:
    std::vector<std::unique_ptr<Search>> threads;
    std::vector<std::unique_ptr<std::thread>> running_threads;

    std::atomic<bool> stop_flag{false};
    std::atomic<size_t> started_threads;

    void launch_worker(Search *worker) {
        std::lock_guard<std::mutex> lock(worker->mutex);
        worker->searching = true;
        worker->cv.notify_all();
    }
};

extern ThreadPool threads;

} // namespace Engine
