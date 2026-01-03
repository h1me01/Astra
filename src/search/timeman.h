#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

#include "tune_params.h"

namespace search {

struct Time {
    int64_t optimum = 0;
    int64_t maximum = 0;
};

class TimeMan {
  private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

  private:
    TimePoint start_time;

  public:
    TimeMan() : start_time(Clock::now()) {}

    void start() {
        start_time = Clock::now();
    }

    int64_t elapsed_time() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_time).count();
    }

    static Time get_optimum(int64_t time_left, int inc, int moves_to_go, int overhead) {
        Time time;

        int mtg = std::min(50, moves_to_go ? moves_to_go : 50);
        int64_t adj_time = std::max(static_cast<int64_t>(1LL), time_left + inc * (mtg - 1) - overhead * (mtg + 2));

        double scale = std::min(                                //
            moves_to_go ? 1.034612 / mtg : 0.029935,            //
            (moves_to_go ? 0.88 : 0.213) * time_left / adj_time //
        );

        time.optimum = time_left * scale;
        time.maximum = time_left * 0.8 - overhead;

        return time;
    }
};

} // namespace search
