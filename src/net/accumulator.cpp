#include "accumulator.h"
#include "../chess/board.h"

namespace NNUE
{

    void AccumTable::refresh(Color view, Board &board)
    {
        assert(view == WHITE || view == BLACK);

        const Square ksq = board.kingSq(view);
        const int ksq_idx = KING_BUCKET[relativeSquare(view, ksq)];
        AccumEntry &entry = entries[view][(fileOf(ksq) > 3) * BUCKET_SIZE + ksq_idx];

        for (Color c : {WHITE, BLACK})
            for (int i = PAWN; i <= KING; i++)
            {
                PieceType pt = PieceType(i);
                Piece pc = makePiece(c, pt);

                const U64 pc_bb = board.getPieceBB(c, pt);
                const U64 entry_bb = entry.piece_bb[c][pt];

                U64 to_set = pc_bb & ~entry_bb;
                while (to_set)
                    nnue.putPiece(entry.acc, entry.acc, pc, popLsb(to_set), ksq, view);

                U64 to_clear = entry_bb & ~pc_bb;
                while (to_clear)
                    nnue.removePiece(entry.acc, entry.acc, pc, popLsb(to_clear), ksq, view);

                entry.piece_bb[c][pt] = pc_bb;
            }

        Accum &acc = board.getAccumulator();
        acc.init[view] = true;

        memcpy(acc.data[view], entry.acc.data[view], sizeof(int16_t) * HIDDEN_SIZE);
    }

    void AccumTable::reset()
    {
        for (Color c : {WHITE, BLACK})
            for (int i = 0; i < 2 * BUCKET_SIZE; i++)
            {
                memset(entries[c][i].piece_bb, 0, sizeof(entries[c][i].piece_bb));
                memcpy(entries[c][i].acc.data[WHITE], nnue.fc1_biases, sizeof(int16_t) * HIDDEN_SIZE);
                memcpy(entries[c][i].acc.data[BLACK], nnue.fc1_biases, sizeof(int16_t) * HIDDEN_SIZE);
            }
    }

} // namespace NNUE
