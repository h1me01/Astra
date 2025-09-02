#include <atomic>
#include <thread>

#include "search.h"

using namespace Chess;

namespace Engine {

class ThreadPool {
  public:
    void launch_workers(const Board &board, Limits limit, int worker_count, bool use_tb);

    void force_stop();

    void stop() {
        stop_flag.store(true);
    }

    bool is_stopped() const {
        return stop_flag.load(std::memory_order_relaxed);
    }

    void start() {
        stop_flag.store(false);
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
    std::vector<std::thread> running_threads;
    std::vector<std::unique_ptr<Search>> threads;

    std::atomic<bool> stop_flag{false};
};

extern ThreadPool threads;

} // namespace Engine
