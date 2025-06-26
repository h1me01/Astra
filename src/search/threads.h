#include <atomic>
#include <thread>

#include "search.h"

namespace Astra {

class ThreadPool {
  private:
    std::vector<std::unique_ptr<Search>> threads;
    std::vector<std::thread> running_threads;

    // stop running all threads
    std::atomic<bool> stop_flag{false};

  public:
    void launchWorkers(const Board &board, Limits limit, int worker_count, bool use_tb);
    void forceStop();

    void stop() {
        stop_flag.store(true);
    }
    void start() {
        stop_flag.store(false);
    }

    bool isStopped() const;

    U64 getTotalNodes() const;
    U64 getTotalTbHits() const;
    int getSelDepth() const;
};

// global variable
extern ThreadPool threads;

} // namespace Astra
