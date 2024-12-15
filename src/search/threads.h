#include <atomic>
#include <thread>
#include "search.h"

namespace Astra
{

    class ThreadPool
    {
    public:
        void launchWorkers(const Board &board, const Limits &limit, int worker_count, bool use_tb);
        void stopAll();

        bool isStopped() const;

        U64 getTotalNodes() const;
        U64 getTotalTbHits() const;
        int getSelDepth() const;
        
        // stop running all threads
        std::atomic<bool> stop{false};
        
    private:
        std::vector<std::unique_ptr<Search>> threads;
        std::vector<std::thread> running_threads;
    };

    // global variable
    extern ThreadPool threads;

} // namespace Astra
