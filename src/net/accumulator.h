#pragma once

#include <array>
#include <cassert>
#include "nnue.h"

namespace NNUE
{
    // idea from stockfish
    struct DirtyPiece
    {
        Piece pc = NO_PIECE;
        Square from = NO_SQUARE;
        Square to = NO_SQUARE;

        DirtyPiece() {}

        DirtyPiece(Piece pc, Square from, Square to)
            : pc(pc), from(from), to(to) {}
    };

    class Accum
    {
    private:
        Square wksq, bksq;

        bool initialized[NUM_COLORS] = {false, false};
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

    public:
        void reset();

        void putPiece(Piece pc, Square to);
        void removePiece(Piece pc, Square from);
        void movePiece(Piece pc, Square from, Square to);

        void update(Accum &prev, Color view);

        void setKingSquares(Square wksq, Square bksq)
        {
            this->wksq = wksq;
            this->bksq = bksq;
        }

        // sets view as initialized
        void markAsInitialized(Color view)
        {
            this->initialized[view] = true;
        }

        // sets view as needing refresh
        void setRefresh(Color view)
        {
            this->needs_refresh[view] = true;
        }

        bool isInitialized(Color view) const { return initialized[view]; }
        bool needsRefresh(Color view) const { return needs_refresh[view]; }

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
