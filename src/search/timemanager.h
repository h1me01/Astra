#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <chrono>
#include "../chess/types.h"

namespace Astra
{
    struct Time {
        int64_t optimum = 0;
        int64_t max = 0;
    };
    
    struct Limits
    {
        Time time;
        U64 nodes = 0;
        int depth = MAX_PLY - 1;
        bool infinite = false;
    };

    class TimeManager
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimeManager() : start_time(Clock::now()) {}

        void start() { start_time = Clock::now(); }

        int64_t elapsedTime() const
        {
            const auto current_time = Clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        }

        static Time getOptimum(int64_t time_left, int inc, int moves_to_go)
        {
            double optimum;

            int overhead = inc ? 0 : 10;
            int mtg = moves_to_go ? std::min(moves_to_go, 50) : 50;

            int64_t adj_time = std::max<int64_t>(1LL, time_left + inc * (mtg - 1) - overhead * (2 + mtg));
            if (moves_to_go == 0)
                optimum = std::min(0.025, 0.2 * time_left / double(adj_time));
            else
                optimum = std::min(0.95 / mtg, 0.88 * time_left / double(adj_time));

            Time time;
            time.optimum = adj_time * optimum;
            time.max = time_left * 0.8 - overhead;
            
            return time;
        }

    private:
        TimePoint start_time;
    };

} // namespace Astra

#endif //TIMEMANAGER_H
