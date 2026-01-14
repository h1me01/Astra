#include "accum.h"
#include "../chess/board.h"

namespace nnue {

// Accum

void Accum::update(Accum& prev, Color view) {
    for (int i = 0; i < dpcs.size(); i++) {
        auto dpc = dpcs[i];
        Square ksq = (view == WHITE) ? dpcs.wksq : dpcs.bksq;

        if (!valid_sq(dpc.from))
            nnue.put(*this, prev, dpc.pc, dpc.to, ksq, view);
        else if (!valid_sq(dpc.to))
            nnue.remove(*this, prev, dpc.pc, dpc.from, ksq, view);
        else
            nnue.move(*this, prev, dpc.pc, dpc.from, dpc.to, ksq, view);
    }

    assert(initialized[view]);
}

// AccumList

void AccumList::refresh(Color view, Board& board) {
    assert(valid_color(view));

    const Square ksq = board.king_sq(view);
    const int ksq_idx = INPUT_BUCKET[rel_sq(view, ksq)];

    auto& entry = entries[view][(sq_file(ksq) > FILE_D) * INPUT_BUCKETS + ksq_idx];

    for (Color c : {WHITE, BLACK}) {
        for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            Piece pc = make_piece(c, pt);

            const U64 pc_bb = board.piece_bb(c, pt);
            const U64 entry_bb = entry(c, pt);

            U64 to_set = pc_bb & ~entry_bb;
            while (to_set)
                nnue.put(entry.accum, entry.accum, pc, pop_lsb(to_set), ksq, view);

            U64 to_clear = entry_bb & ~pc_bb;
            while (to_clear)
                nnue.remove(entry.accum, entry.accum, pc, pop_lsb(to_clear), ksq, view);

            entry(c, pt) = pc_bb;
        }
    }

    auto& accum = back();
    std::memcpy(accum.data[view], entry.accum.data[view], sizeof(int16_t) * FT_SIZE);
    accum.initialized[view] = true;
}

} // namespace nnue
