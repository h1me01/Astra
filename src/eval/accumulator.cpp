#include "accumulator.h"
#include "../chess/board.h"

namespace NNUE
{

    void AccumulatorTable::refresh(Color view, Board &board)
    {
        Square ksq = board.kingSq(view);
        const bool king_side = fileOf(ksq) > 3;

        AccumulatorEntry &entry = entries[view][king_side * 10 + KING_BUCKET[relativeSquare(view, ksq)]];
    
        for (Color c : {WHITE, BLACK})
            for (int i = PAWN; i <= KING; i++)
            {
                PieceType pt = PieceType(i);
                Piece pc = makePiece(c, pt);

                U64 pc_bb = board.getPieceBB(c, pt);
                U64 entry_bb = entry.piece_bb[c][pt];

                U64 to_set = pc_bb & ~entry_bb;
                U64 to_clear = entry_bb & ~pc_bb;

                while (to_set)
                    nnue.putPiece(entry.acc, pc, popLsb(to_set), ksq, ksq, view);

                while (to_clear)
                    nnue.removePiece(entry.acc, pc, popLsb(to_clear), ksq, ksq, view);

                entry.piece_bb[c][pt] = pc_bb;
            }

        Accumulator &acc = board.getAccumulator();
        std::memcpy(acc.data[view], entry.acc.data[view], sizeof(int16_t) * HIDDEN_SIZE);
    }

    void AccumulatorTable::reset()
    {
        for (Color c : {WHITE, BLACK})
            for (int i = 0; i < 2 * BUCKET_SIZE; i++)
                std::memcpy(entries[c][i].acc.data[c], nnue.fc1_biases, sizeof(int16_t) * HIDDEN_SIZE);
    }

} // namespace NNUE
