#include "timeman.h"

namespace Astra
{

    Time getOptimum(int64_t time_left, int inc, int moves_to_go, int overhead)
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

} // namespace Astra
