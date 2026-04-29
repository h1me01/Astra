#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace astra::search {

struct Time {
    int64_t optimum = 0;
    int64_t maximum = 0;
};

struct Limits {
    Time time;
    uint64_t nodes = 0;
    int depth = MAX_PLY - 1;
    int multipv = 1;
    bool infinite = false;
    bool minimal = false;
    std::vector<std::string> search_moves{};
};

class TimeMan {
    using Clock = std::chrono::steady_clock;

  public:
    TimeMan()
        : start_time_(Clock::now()) {}

    void start() { start_time_ = Clock::now(); }

    int64_t elapsed_time() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start_time_).count();
    }

    static Time get_optimum(int64_t time_left, int inc, int moves_to_go, int overhead) {
        Time time;

        int mtg = (moves_to_go > 0) ? std::min(50, moves_to_go) : 50;
        int64_t adj_time = time_left + inc * (mtg - 1) - overhead * (mtg + 2);

        double scale = std::min(
            moves_to_go ? 1.034612 / mtg : 0.029935,
            (moves_to_go ? 0.88 : 0.213) * time_left / std::max<int64_t>(1LL, adj_time)
        );

        time.optimum = time_left * scale;
        time.maximum = time_left * 0.8 - overhead;

        return time;
    }

  private:
    Clock::time_point start_time_;
};

} // namespace astra::search
