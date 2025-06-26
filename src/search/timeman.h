#pragma once

#include <chrono>

#include "../chess/types.h"

using namespace Chess;

namespace Astra {

struct Time {
    int64_t optimum = 0;
    int64_t max = 0;
};

struct Limits {
    Time time;
    U64 nodes = 0;
    int depth = MAX_PLY - 1;
    int multipv = 1;
    bool infinite = false;
};

class TimeMan {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

  private:
    TimePoint start_time;

  public:
    TimeMan() : start_time(Clock::now()) {}

    void start() {
        start_time = Clock::now();
    }

    int64_t elapsedTime() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_time).count();
    }

    static Time getOptimum(int64_t time_left, int inc, int moves_to_go, int overhead) {
        Time time;

        int mtg = moves_to_go ? std::min(50, moves_to_go) : 50;
        int64_t adj_time = std::max<int64_t>(1LL, time_left + inc * mtg / 2 - overhead * (moves_to_go ? 1 : mtg));

        if(moves_to_go == 0)
            time.optimum = adj_time * 0.05;
        else
            time.optimum = std::min(time_left * 0.5, adj_time * 0.9 / std::max(1.0, mtg / 2.0));

        time.max = time_left * 0.8 - overhead;

        return time;
    }
};

} // namespace Astra
