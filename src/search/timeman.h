#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <chrono>
#include "../chess/types.h"

using namespace Chess;

namespace Astra
{
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct Time
    {
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

    class TimeMan
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimeMan() : start_time(Clock::now()) {}

        void start() { start_time = Clock::now(); }

        int64_t elapsedTime() const
        {
            return std::chrono::duration_cast<std::chrono::milliseconds>( Clock::now() - start_time).count();
        }

        static Time getOptimum(int64_t time_left, int inc, int moves_to_go, int overhead)
        {
            Time time;

            int mtg = moves_to_go ? moves_to_go : 50;

            int64_t adj_time = std::max<int64_t>(1LL, time_left + inc * mtg - overhead * (moves_to_go ? 1 : mtg));

            if (moves_to_go == 0)
                time.optimum = adj_time * 0.0575;
            else
                time.optimum = std::min(time_left * 0.9, (adj_time * 0.9) / std::max(1.0, mtg / 2.5));

            time.max = 0.8 * time_left - overhead;

            return time;
        }

    private:
        TimePoint start_time;
    };


} // namespace Astra

#endif // TIMEMANAGER_H
