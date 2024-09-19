#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <chrono>
#include "../chess/types.h"

using namespace Chess;

namespace Astra {

    class TimeManager {
    public:
        unsigned int divider = 50;

        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        TimeManager() : start_time(Clock::now()) {
        }

        // get time in ms
        unsigned int getTime(int time, int inc, int moves_to_go) const {
            unsigned int time_ms;

            if (moves_to_go == 0) {
                time_ms = inc + (time - time / divider) / divider;
            } else {
                time_ms = inc + (time - time / (moves_to_go + 3)) / (0.7 * moves_to_go + 3);
            }

            return time_ms;
        }

        void start() {
            start_time = Clock::now();
        }

        // set total time allowed for a game (in ms)
        void setTimePerMove(const unsigned int ms) {
            tpm = ms;
        }

        bool isTimeExceeded() const {
            return elapsedTime() > tpm;
        }

        int elapsedTime() const {
            const auto current_time = Clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        }

    private:
        TimePoint start_time;
        unsigned int tpm{};
    };

} // namespace Astra

#endif //TIMEMANAGER_H
