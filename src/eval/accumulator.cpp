#include "accumulator.h"
#include "../chess/board.h"

namespace NNUE
{

    void AccumulatorTable::use(Color view, Board &board)
    {
        const Square ksq = board.kingSq(view);
        const bool king_side = fileOf(ksq) > 3;
        const int king_idx = KING_BUCKET[ksq];

        const int entry_idx = king_side * 10 + king_idx;

        AccumulatorEntry &entry = entries[view][entry_idx];
        Accumulator &acc = board.getAccumulator();

        for (Color c : {WHITE, BLACK})
            for (int pt = PAWN; pt <= KING; pt++)
            {
                U64 pc_bb = board.getPieceBB(c, PieceType(pt));
                U64 entry_bb = entry.piece_bb[c][PieceType(pt)];

                U64 to_set = pc_bb & ~entry_bb;
                U64 to_clear = entry_bb & ~pc_bb;

                while (to_set)
                {
                    Square sq = popLsb(to_set);
                    nnue.putPiece(acc, makePiece(c, PieceType(pt)), sq, ksq, ksq);
                }

                while (to_clear)
                {
                    Square sq = popLsb(to_clear);
                    nnue.removePiece(acc, makePiece(c, PieceType(pt)), sq, ksq, ksq);
                }

                entry.piece_bb[c][PieceType(pt)] = pc_bb;
            }
    }

    void AccumulatorTable::reset()
    {
        for (Color c : {WHITE, BLACK})
            for (int s = 0; s < 2 * BUCKET_SIZE; s++)
                std::memcpy(entries[c][s].acc.data[c], nnue.fc1_biases, sizeof(int16_t) * HIDDEN_SIZE);
    }

} // namespace NNUE
