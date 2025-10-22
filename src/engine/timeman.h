#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>

#include "tune_params.h"

namespace Engine {

struct Time {
    int64_t optimum = 0;
    int64_t maximum = 0;
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

    int64_t elapsed_time() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_time).count();
    }

    bool should_stop(           //
        Limits limits,          //
        int stability,          //
        Score prev_score_diff,  //
        Score pprev_score_diff, //
        double node_ratio       //
    ) const {
        if(!limits.time.optimum)
            return false;

        double stability_factor = (tm_stability_base / 100.0) - stability * (tm_stability_mult / 1000.0);

        // adjust time optimum based on last score
        double result_change_factor = (tm_results_base / 100.0)                       //
                                      + (tm_results_mult1 / 1000.0) * prev_score_diff //
                                      + (tm_results_mult2 / 1000.0) * pprev_score_diff;

        result_change_factor = std::clamp(result_change_factor, tm_results_min / 100.0, tm_results_max / 100.0);

        // adjust time optimum based on node count
        double node_count_factor = (1.0 - node_ratio) * (tm_node_mult / 100.0) + (tm_node_base / 100.0);

        // check if we should stop
        return (elapsed_time() > limits.time.optimum * stability_factor * result_change_factor * node_count_factor);
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

} // namespace Engine
