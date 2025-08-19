#pragma once

#include <chrono>

#include "../chess/types.h"

using namespace Chess;

namespace Search {

struct Time {
    int64_t maximum = 0;
};

struct Limits {
    Time time;
    U64 nodes = 0;
    int depth = MAX_PLY - 1;
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
};

} // namespace Search
