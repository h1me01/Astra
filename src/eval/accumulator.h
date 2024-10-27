#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include "nnue.h"
#include <cassert>

using namespace Chess;

namespace NNUE
{
    class Accumulators
    {
    public:
        Accumulators() : index(0) {}

        int size() const
        {
            assert(index >= 0 && index < MAX_PLY);
            return index;
        }

        void clear() { index = 0; }

        void push()
        {
            assert(index >= 0 && index < MAX_PLY - 1);
            index++;
            accumulators[index] = accumulators[index - 1];
        }

        void pop()
        {
            assert(index > 0);
            index--;
        }

        Accumulator& back()
        {
            assert(index >= 0 && index < MAX_PLY);
            return accumulators[index];
        }

    private:
        int index;
        std::array<Accumulator, MAX_PLY> accumulators{};
    };

} // namespace NNUE

#endif // ACCUMULATOR_H
