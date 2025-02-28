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

namespace NNUE
{

    struct Accumulator
    {
        bool initialized[NUM_COLORS]{false, false};
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

            accumulators[index].initialized[WHITE] = false;
            accumulators[index].initialized[BLACK] = false;
        }

        void pop()
        {
            assert(index > 0);
            index--;

            accumulators[index].initialized[WHITE] = true;
            accumulators[index].initialized[BLACK] = true;
        }

        Accumulator &back(Color c)
        {
            if (c != BLACK)
                copyAcc(WHITE);
            if (c != WHITE)
                copyAcc(BLACK);

            return accumulators[index];
        }

    private:
        void copyAcc(Color c)
        {
            if (accumulators[index].initialized[c])
                return;

            std::memcpy(accumulators[index].data[c], accumulators[index - 1].data[c], sizeof(int16_t) * HIDDEN_SIZE);
            accumulators[index].initialized[c] = true;
        }

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

} // namespace NNUE

#endif // NNUE_H
