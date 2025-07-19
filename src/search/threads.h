#include <atomic>
#include <thread>

#include "search.h"

using namespace Chess;

namespace Astra {

class ThreadPool {
  public:
    void force_stop();
    bool is_stopped() const;

    U64 get_nodes() const;
    U64 get_tb_hits() const;

    void launch_workers(const Board &board, Limits limit, int worker_count, bool use_tb);

    void stop() {
        stop_flag.store(true);
    }

    void start() {
        stop_flag.store(false);
    }

  private:
    std::vector<std::unique_ptr<Search>> threads;
    std::vector<std::thread> running_threads;

    // stop running all threads
    std::atomic<bool> stop_flag{false};
};

// global variable
extern ThreadPool threads;

} // namespace Astra
