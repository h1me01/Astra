#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <chrono>
#include "../chess/types.h"

using namespace Chess;

namespace Astra {

    struct Limits {
        int64_t time = 0;
        U64 nodes = 0;
        int depth = MAX_PLY - 1;
        bool infinite = false;
    };

    class TimeManager {
    public:
        int divider = 50;

        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimeManager() : start_time(Clock::now()) {
        }

        void init(const Limits& limit) {
            this->limit = limit;
        }

        void start() {
            start_time = Clock::now();
        }

        int64_t elapsedTime() const {
            const auto current_time = Clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        }

        // get time in ms
        int64_t getOptimal(int64_t time, int inc, int moves_to_go) const {
            int64_t optimal;
            if (moves_to_go == 0) {
                optimal = inc + (time - time / divider) / divider;
            } else {
                optimal = inc + (time - time / (moves_to_go + 3)) / (0.7 * moves_to_go + 3);
            }

            return optimal;
        }

        bool isLimitReached(int depth, U64 nodes) const {
            if (limit.infinite) {
                return false;
            }

            if (limit.time != 0 && elapsedTime() > limit.time) {
                return true;
            }

            if (limit.nodes != 0 && nodes >= limit.nodes) {
                return true;
            }

            if (limit.depth != 0 && depth > limit.depth) {
                return true;
            }

            return false;
        }

    private:
        TimePoint start_time;
        Limits limit;

    };

} // namespace Astra

#endif //TIMEMANAGER_H
