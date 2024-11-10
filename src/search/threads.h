#include <thread>
#include "search.h"

namespace Astra
{

    class ThreadPool
    {
    public:
        ThreadPool() : stop(false) {}

        void launchWorkers(const Board &board, const Limits &limit, int worker_count, bool use_tb);
        void stopAll();

        U64 getTotalNodes() const;
        U64 getTotalTbHits() const;
        uint8_t getSelDepth() const;

        // stop running all threads
        bool stop;

    private:
        std::vector<Search> threads;
        std::vector<std::thread> running_threads;
    };

    // global variable
    extern ThreadPool threads;

} // namespace Astra
