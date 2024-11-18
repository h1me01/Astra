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

    Time getOptimum(int64_t time_left, int inc, int moves_to_go, int overhead);

} // namespace Astra

#endif // TIMEMANAGER_H
