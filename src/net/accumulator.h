#pragma once

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

    struct Accum
    {
        bool init[NUM_COLORS] = {false, false};
        alignas(ALIGNMENT) int16_t data[NUM_COLORS][HIDDEN_SIZE];
    };

    class Accumulators
    {
    public:
        Accumulators() : index(0) {}

        int getIndex() const { return index; }

        void clear() { index = 0; }

        void increment()
        {
            index++;
            assert(index < MAX_PLY + 1);

            accumulators[index].init[WHITE] = false;
            accumulators[index].init[BLACK] = false;
        }

        void decrement()
        {
            assert(index > 0);
            index--;

            accumulators[index].init[WHITE] = false;
            accumulators[index].init[BLACK] = false;
        }

        Accum &back() { return accumulators[index]; }
        Accum &operator[](int idx) { return accumulators[idx]; }

    private:
        int index;
        std::array<Accum, MAX_PLY + 1> accumulators{};
    };

    // idea from koivisto
    struct AccumEntry
    {
        U64 piece_bb[NUM_COLORS][NUM_PIECE_TYPES]{};
        Accum acc{};
    };

    struct AccumTable
    {
        AccumEntry entries[NUM_COLORS][2 * BUCKET_SIZE]{};

        void refresh(Color view, Board &board);
        void reset();
    };

} // namespace NNUE
