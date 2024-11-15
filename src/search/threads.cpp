#include "threads.h"

namespace Astra
{
    bool ThreadPool::isStopped() const
    {
        return stop.load(std::memory_order_relaxed);
    }

    void ThreadPool::launchWorkers(const Board &board, const Limits &limit, int worker_count, bool use_tb)
    {
        stopAll(); // stop all threads

        stop = false;

        threads.clear();
        for (int i = 0; i < worker_count; i++)
        {
            Search thread(STARTING_FEN);
            thread.id = i;
            thread.board = board;
            thread.limit = limit;
            thread.use_tb = use_tb;
            threads.emplace_back(thread);
        }

        for (int i = 0; i < worker_count; i++)
            running_threads.emplace_back(&Search::start, std::ref(threads[i]));   
    }

    void ThreadPool::stopAll()
    {
        stop = true;

        for (auto &th : running_threads)
            if (th.joinable())
                th.join();

        threads.clear();
        running_threads.clear();
    }

    U64 ThreadPool::getTotalNodes() const
    {
        U64 total_nodes = 0;
        for (const auto &t : threads)
            total_nodes += t.nodes;
        return total_nodes;
    }

    U64 ThreadPool::getTotalTbHits() const
    {
        U64 total_tb_hits = 0;
        for (const auto &t : threads)
            total_tb_hits += t.tb_hits;
        return total_tb_hits;
    }

    uint8_t ThreadPool::getSelDepth() const
    {
        uint8_t max_sel_depth = 0;
        for (const auto &t : threads)
            max_sel_depth = std::max(max_sel_depth, t.sel_depth);
        return max_sel_depth;
    }

    // global variable
    ThreadPool threads;

} // namespace Astra
