#include "accumulator.h"
#include "../chess/board.h"

namespace NNUE
{

    // Accum
    void Accum::reset()
    {
        initialized[WHITE] = needs_refresh[WHITE] = false;
        initialized[BLACK] = needs_refresh[BLACK] = false;

        num_dpcs = 0;
    }

    void Accum::putPiece(Piece pc, Square to)
    {
        assert(num_dpcs < 4);
        dpcs[num_dpcs++] = DirtyPiece(pc, NO_SQUARE, to);
    }

    void Accum::removePiece(Piece pc, Square from)
    {
        assert(num_dpcs < 4);
        dpcs[num_dpcs++] = DirtyPiece(pc, from, NO_SQUARE);
    }

    void Accum::movePiece(Piece pc, Square from, Square to)
    {
        assert(num_dpcs < 4);
        dpcs[num_dpcs++] = DirtyPiece(pc, from, to);
    }

    void Accum::update(Accum &prev, Color view)
    {
        for (int i = 0; i < num_dpcs; i++)
        {
            DirtyPiece dpc = dpcs[i];
            Square ksq = (view == WHITE) ? wksq : bksq;

            if (dpc.from == NO_SQUARE)
                nnue.putPiece(*this, prev, dpc.pc, dpc.to, ksq, view);
            else if (dpc.to == NO_SQUARE)
                nnue.removePiece(*this, prev, dpc.pc, dpc.from, ksq, view);
            else
                nnue.movePiece(*this, prev, dpc.pc, dpc.from, dpc.to, ksq, view);
        }

        assert(initialized[view] == true);
    }

    // AccumTable

    void AccumTable::refresh(Color view, Board &board)
    {
        assert(view == WHITE || view == BLACK);

        const Square ksq = board.kingSq(view);
        const int ksq_idx = KING_BUCKET[relSquare(view, ksq)];
        AccumEntry &entry = entries[view][(fileOf(ksq) > 3) * BUCKET_SIZE + ksq_idx];

        Accum &entry_acc = entry.getAccum();

        for (Color c : {WHITE, BLACK})
            for (int i = PAWN; i <= KING; i++)
            {
                PieceType pt = PieceType(i);
                Piece pc = makePiece(c, pt);

                const U64 pc_bb = board.getPieceBB(c, pt);
                const U64 entry_bb = entry.getPieceBB(c, pt);

                U64 to_set = pc_bb & ~entry_bb;
                while (to_set)
                    nnue.putPiece(entry_acc, entry_acc, pc, popLsb(to_set), ksq, view);

                U64 to_clear = entry_bb & ~pc_bb;
                while (to_clear)
                    nnue.removePiece(entry_acc, entry_acc, pc, popLsb(to_clear), ksq, view);

                entry.setPieceBB(c, pt, pc_bb);
            }

        Accum &acc = board.getAccumulator();
        acc.markAsInitialized(view);

        memcpy(acc.getData(view), entry_acc.getData(view), sizeof(int16_t) * HIDDEN_SIZE);
    }

    void AccumTable::reset()
    {
        for (Color c : {WHITE, BLACK})
            for (int i = 0; i < 2 * BUCKET_SIZE; i++)
                entries[c][i].reset();
    }

} // namespace NNUE
