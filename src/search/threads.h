#include <atomic>
#include <thread>

#include "search.h"

using namespace Chess;

namespace Astra {

class ThreadPool {
  private:
    std::vector<std::unique_ptr<Search>> threads;
    std::vector<std::thread> running_threads;

    // stop running all threads
    std::atomic<bool> stop_flag{false};

  public:
    void forceStop();
    bool isStopped() const;
    U64 getTotalNodes() const;
    U64 getTotalTbHits() const;
    int getSelDepth() const;

    void launchWorkers(const Board &board, Limits limit, int worker_count, bool use_tb);

    void stop() {
        stop_flag.store(true);
    }

    void start() {
        stop_flag.store(false);
    }
};

// global variable
extern ThreadPool threads;

} // namespace Astra
