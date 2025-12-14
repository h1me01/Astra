#include "accum.h"
#include "../chess/board.h"

namespace nnue {

// Accum

void Accum::update(Accum &prev, Color view) {
    for(int i = 0; i < dirty_pcs.size(); i++) {
        auto dpc = dirty_pcs[i];
        Square ksq = (view == WHITE) ? wksq : bksq;

        if(!valid_sq(dpc.from))
            nnue.put(*this, prev, dpc.pc, dpc.to, ksq, view);
        else if(!valid_sq(dpc.to))
            nnue.remove(*this, prev, dpc.pc, dpc.from, ksq, view);
        else
            nnue.move(*this, prev, dpc.pc, dpc.from, dpc.to, ksq, view);
    }

    assert(is_initialized(view));
}

// AccumTable

void AccumTable::reset() {
    for(Color c : {WHITE, BLACK})
        for(int i = 0; i < 2 * INPUT_BUCKETS; i++)
            entries[c][i].reset();
}

void AccumTable::refresh(Color view, Board &board, Accum &accum) {
    assert(valid_color(view));

    const Square ksq = board.get_king_sq(view);
    const int ksq_idx = INPUT_BUCKET[rel_sq(view, ksq)];
    AccumEntry &entry = entries[view][(sq_file(ksq) > 3) * INPUT_BUCKETS + ksq_idx];

    Accum &entry_acc = entry.get_accum();

    for(Color c : {WHITE, BLACK}) {
        for(PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            Piece pc = make_piece(c, pt);

            const U64 pc_bb = board.get_piece_bb(c, pt);
            const U64 entry_bb = entry.get_piece_bb(c, pt);

            U64 to_set = pc_bb & ~entry_bb;
            while(to_set)
                nnue.put(entry_acc, entry_acc, pc, pop_lsb(to_set), ksq, view);

            U64 to_clear = entry_bb & ~pc_bb;
            while(to_clear)
                nnue.remove(entry_acc, entry_acc, pc, pop_lsb(to_clear), ksq, view);

            entry.set_piecebb(c, pt, pc_bb);
        }
    }

    memcpy(accum.get_data(view), entry_acc.get_data(view), sizeof(int16_t) * FT_SIZE);
    accum.set_initialized(view);
}

} // namespace nnue
