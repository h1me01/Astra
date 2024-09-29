#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include "nnue.h"
#include <cassert>
#include <cstring> // std::memcpy

using namespace Chess;

namespace NNUE {

    class Accumulators {
    public:
        Accumulators() : index(0) {
            allocateMemory();

            for (int j = 0; j < HIDDEN_SIZE; j++) {
                accumulators[0][WHITE][j] = nnue.fc1_biases[j];
                accumulators[0][BLACK][j] = nnue.fc1_biases[j];
            }
        }

        ~Accumulators() {
            clearMemory();
        }

        Accumulators &operator=(const Accumulators &other) {
            if (this != &other) {
                clearMemory();
                index = other.index;
                allocateMemory();

                for (int i = 0; i <= index; i++) {
                    std::memcpy(accumulators[i][0], other.accumulators[i][0], HIDDEN_SIZE * sizeof(int16_t));
                    std::memcpy(accumulators[i][1], other.accumulators[i][1], HIDDEN_SIZE * sizeof(int16_t));
                }
            }
            return *this;
        }

        int size() const {
            assert(index >= 0 && index < MAX_PLY);
            return index;
        }

        void clear() {
            index = 0;
        }

        void push() {
            assert(index >= 0 && index < MAX_PLY - 1);
            index++;

            size_t size = HIDDEN_SIZE * sizeof(accumulators[0][0][0]);
            std::memcpy(accumulators[index][0], accumulators[index - 1][0], size);
            std::memcpy(accumulators[index][1], accumulators[index - 1][1], size);
        }

        void pop() {
            assert(index > 0);
            index--;
        }

        Accumulator back() const {
            assert(index >= 0 && index < MAX_PLY);
            return accumulators[index];
        }

    private:
        int index;
        std::array<Accumulator, MAX_PLY> accumulators{};

        void allocateMemory() {
            for (int i = 0; i < MAX_PLY; i++) {
                accumulators[i][0] = new int16_t[HIDDEN_SIZE]{};
                accumulators[i][1] = new int16_t[HIDDEN_SIZE]{};
            }
        }

        void clearMemory() const {
            for (const auto &acc: accumulators) {
                delete[] acc[0];
                delete[] acc[1];
            }
        }
    };

} // namespace NNUE

#endif // ACCUMULATOR_H