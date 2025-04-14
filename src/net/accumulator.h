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

        int num_dps = 0;
        // an accumulator can update at max only 4 pieces per move:
        // such case might be pawn captures piece on promotion rank:
        //  1. remove captured piece
        //  2. move pawn to target square
        //  3. add promotion piece to target square
        //  4. remove pawn from target square
        DirtyPiece dirty_pieces[4]{};

        void reset()
        {
            init[WHITE] = needs_refresh[WHITE] = false;
            init[BLACK] = needs_refresh[BLACK] = false;

            num_dps = 0;
        }

        void putPiece(Piece pc, Square to, Square wksq, Square bksq)
        {
            assert(num_dps < 4);
            dirty_pieces[num_dps++] = DirtyPiece(pc, NO_SQUARE, to, wksq, bksq);
        }

        void removePiece(Piece pc, Square from, Square wksq, Square bksq)
        {
            assert(num_dps < 4);
            dirty_pieces[num_dps++] = DirtyPiece(pc, from, NO_SQUARE, wksq, bksq);
        }

        void movePiece(Piece pc, Square from, Square to, Square wksq, Square bksq)
        {
            assert(num_dps < 4);
            dirty_pieces[num_dps++] = DirtyPiece(pc, from, to, wksq, bksq);
        }

        void update(Accum &prev, Color view)
        {
            for (int i = 0; i < num_dps; i++)
            {
                DirtyPiece dp = dirty_pieces[i];
                Square ksq = (view == WHITE) ? dp.wksq : dp.bksq;

                if (dp.from == NO_SQUARE)
                    nnue.putPiece(*this, prev, dp.pc, dp.to, ksq, view);
                else if (dp.to == NO_SQUARE)
                    nnue.removePiece(*this, prev, dp.pc, dp.from, ksq, view);
                else
                    nnue.movePiece(*this, prev, dp.pc, dp.from, dp.to, ksq, view);
            }            
        }
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

            accumulators[index].reset();
        }

        void decrement()
        {
            assert(index > 0);
            index--;
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
