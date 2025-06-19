#include "accumulator.h"
#include "../chess/board.h"

namespace NNUE {

void AccumTable::refresh(Board &board, Color view) {
    assert(view == WHITE || view == BLACK);

    const Square ksq = board.kingSq(view);
    const int ksq_idx = KING_BUCKET[relSquare(view, ksq)];
    AccumEntry &entry = entries[view][(fileOf(ksq) > 3) * BUCKET_SIZE + ksq_idx];

    Accum &entry_acc = entry.getAccum();

    for(Color c : {WHITE, BLACK}) {
        for(int i = PAWN; i <= KING; i++) {
            PieceType pt = PieceType(i);
            Piece pc = makePiece(c, pt);

            const U64 pc_bb = board.getPieceBB(c, pt);
            const U64 entry_bb = entry.getPieceBB(c, pt);

            U64 to_set = pc_bb & ~entry_bb;
            while(to_set)
                nnue.putPiece(entry_acc, entry_acc, pc, popLsb(to_set), ksq, view);

            U64 to_clear = entry_bb & ~pc_bb;
            while(to_clear)
                nnue.removePiece(entry_acc, entry_acc, pc, popLsb(to_clear), ksq, view);

            entry.setPieceBB(c, pt, pc_bb);
        }
    }

    Accum &acc = board.getAccumulator();
    acc.markAsInitialized(view);

    memcpy(acc.getData(view), entry_acc.getData(view), sizeof(int16_t) * L1_SIZE);
}

void AccumTable::reset() {
    for(Color c : {WHITE, BLACK})
        for(int i = 0; i < 2 * BUCKET_SIZE; i++)
            entries[c][i].reset();
}

} // namespace NNUE
