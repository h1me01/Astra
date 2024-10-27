#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <chrono>
#include "../chess/types.h"

namespace Astra
{
    struct Limits
    {
        int64_t time = 0;
        U64 nodes = 0;
        int depth = 99;
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

        static int64_t getOptimum(int64_t time, int inc, int moves_to_go)
        {
            double optimum;

            int overhead = inc ? 0 : 10;
            int mtg = moves_to_go ? std::min(moves_to_go, 50) : 50;

            int64_t timeLeft = std::max<int64_t>(1, time + inc * (mtg - 1) - overhead * (2 + mtg));
            if (moves_to_go == 0)
                optimum = std::min(0.025, 0.2 * time / timeLeft);
            else
                optimum = std::min(0.95 / mtg, 0.88 * time / timeLeft);

            return optimum * timeLeft;
        }

    private:
        TimePoint start_time;
    };

} // namespace Astra

#endif //TIMEMANAGER_H
