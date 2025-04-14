#pragma once

#include <array>
#include <cassert>
#include "nnue.h"

namespace NNUE
{

    struct DirtyPiece
    {
        Piece pc;
        Square from, to, wksq, bksq;

        DirtyPiece() {}
        DirtyPiece(Piece pc, Square from, Square to, Square wksq, Square bksq)
            : pc(pc), from(from), to(to), wksq(wksq), bksq(bksq) {}
    };

    struct Accum
    {
        bool init[NUM_COLORS] = {false, false};
        bool needs_refresh[NUM_COLORS] = {false, false};

        alignas(ALIGNMENT) int16_t data[NUM_COLORS][HIDDEN_SIZE];

        int num_dpcs = 0;
        // an accumulator can update at max only 4 pieces per move:
        // such case might be pawn captures piece on promotion rank:
        //  1. remove captured piece
        //  2. move pawn to target square
        //  3. add promotion piece to target square
        //  4. remove pawn from target square
        DirtyPiece dpcs[4]{};

        void reset()
        {
            init[WHITE] = needs_refresh[WHITE] = false;
            init[BLACK] = needs_refresh[BLACK] = false;

            num_dpcs = 0;
        }

        void putPiece(Piece pc, Square to, Square wksq, Square bksq)
        {
            assert(num_dpcs < 4);
            dpcs[num_dpcs++] = DirtyPiece(pc, NO_SQUARE, to, wksq, bksq);
        }

        void removePiece(Piece pc, Square from, Square wksq, Square bksq)
        {
            assert(num_dpcs < 4);
            dpcs[num_dpcs++] = DirtyPiece(pc, from, NO_SQUARE, wksq, bksq);
        }

        void movePiece(Piece pc, Square from, Square to, Square wksq, Square bksq)
        {
            assert(num_dpcs < 4);
            dpcs[num_dpcs++] = DirtyPiece(pc, from, to, wksq, bksq);
        }

        void update(Accum &prev, Color view)
        {
            for (int i = 0; i < num_dpcs; i++)
            {
                DirtyPiece dpc = dpcs[i];
                Square ksq = (view == WHITE) ? dpc.wksq : dpc.bksq;

                if (dpc.from == NO_SQUARE)
                    nnue.putPiece(*this, prev, dpc.pc, dpc.to, ksq, view);
                else if (dpc.to == NO_SQUARE)
                    nnue.removePiece(*this, prev, dpc.pc, dpc.from, ksq, view);
                else
                    nnue.movePiece(*this, prev, dpc.pc, dpc.from, dpc.to, ksq, view);
            }            

            assert(init[view] == true);
        }
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
