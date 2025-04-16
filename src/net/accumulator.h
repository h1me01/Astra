#pragma once

#include <array>
#include <cassert>
#include "nnue.h"

namespace NNUE
{

    class Accum
    {
    private:
        bool initialized[NUM_COLORS] = {false, false};
        alignas(ALIGNMENT) int16_t data[NUM_COLORS][HIDDEN_SIZE];

    public:
        void reset()
        {
            initialized[WHITE] = false;
            initialized[BLACK] = false;
        }

        void markAsInitialized(Color view)
        {
            this->initialized[view] = true;
        }

        bool isInitialized(Color view) const { return initialized[view]; }

        int16_t *getData(Color view) { return data[view]; }
    };

    class Accumulators
    {
    public:
        Accumulators() : counter(0) {}

        int size() const { return counter + 1; }

        void clear() { counter = 0; }

        void increment()
        {
            counter++;
            assert(counter < MAX_PLY + 1);

            accumulators[counter].reset();
        }

        void decrement()
        {
            assert(counter > 0);
            counter--;
        }

        Accum &back() { return accumulators[counter]; }
        Accum &operator[](int idx) { return accumulators[idx]; }

    private:
        int counter;
        std::array<Accum, MAX_PLY + 1> accumulators{};
    };

    // idea from koivisto
    class AccumEntry
    {
    private:
        U64 piece_bb[NUM_COLORS][NUM_PIECE_TYPES]{};
        Accum acc{};

    public:
        void reset()
        {
            memset(piece_bb, 0, sizeof(piece_bb));
            nnue.initAccum(acc);
        }

        void setPieceBB(Color c, PieceType pt, U64 bb)
        {
            piece_bb[c][pt] = bb;
        }

        U64 getPieceBB(Color c, PieceType pt) const { return piece_bb[c][pt]; }
        Accum &getAccum() { return acc; }
    };

    class AccumTable
    {
    private:
        AccumEntry entries[NUM_COLORS][2 * BUCKET_SIZE]{};

    public:
        void refresh(Color view, Board &board);
        void reset();
    };

} // namespace NNUE
