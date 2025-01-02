#ifndef ACCUMULATOR_H
#define ACCUMULATOR_H

#include <array>
#include <cassert>
#include "nnue.h"
#include "../chess/types.h"

namespace Chess
{
    class Board;
}

using namespace Chess;

namespace NNUE
{

    struct Accumulator
    {
        alignas(ALIGNMENT) int16_t data[NUM_COLORS][HIDDEN_SIZE];
    };

    class Accumulators
    {
    public:
        Accumulators() : index(0) {}

        int size() const { return index; }

        void clear() { index = 0; }

        void push()
        {
            index++;
            assert(index < MAX_PLY + 1);
            accumulators[index] = accumulators[index - 1];
        }

        void pop()
        {
            assert(index > 0);
            index--;
        }

        Accumulator &back() { return accumulators[index]; }

    private:
        int index;
        std::array<Accumulator, MAX_PLY + 1> accumulators{};
    };

    // idea from koivisto
    struct AccumulatorEntry
    {
        U64 piece_bb[NUM_COLORS][NUM_PIECE_TYPES]{};
        Accumulator acc{};
    };

    struct AccumulatorTable
    {
        AccumulatorEntry entries[NUM_COLORS][2 * BUCKET_SIZE]{};

        void refresh(Color view, Board &board);
        void reset();
    };

}

#endif // NNUE_H
