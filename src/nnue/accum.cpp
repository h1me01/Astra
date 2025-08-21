#include "accum.h"
#include "../chess/board.h"

namespace NNUE {

// Accum

void Accum::reset() {
    initialized[WHITE] = m_needs_refresh[WHITE] = false;
    initialized[BLACK] = m_needs_refresh[BLACK] = false;

    num_dpcs = 0;
}

void Accum::put_piece(Piece pc, Square to, Square wksq, Square bksq) {
    assert(num_dpcs < 4);
    this->wksq = wksq;
    this->bksq = bksq;
    dpcs[num_dpcs++] = DirtyPiece(pc, NO_SQUARE, to);
}

void Accum::remove_piece(Piece pc, Square from, Square wksq, Square bksq) {
    assert(num_dpcs < 4);
    this->wksq = wksq;
    this->bksq = bksq;
    dpcs[num_dpcs++] = DirtyPiece(pc, from, NO_SQUARE);
}

void Accum::move_piece(Piece pc, Square from, Square to, Square wksq, Square bksq) {
    assert(num_dpcs < 4);
    this->wksq = wksq;
    this->bksq = bksq;
    dpcs[num_dpcs++] = DirtyPiece(pc, from, to);
}

void Accum::update(Accum &prev, Color view) {
    for(int i = 0; i < num_dpcs; i++) {
        DirtyPiece dpc = dpcs[i];
        Square ksq = (view == WHITE) ? wksq : bksq;

        if(dpc.from == NO_SQUARE)
            nnue.put(*this, prev, dpc.pc, dpc.to, ksq, view);
        else if(dpc.to == NO_SQUARE)
            nnue.remove(*this, prev, dpc.pc, dpc.from, ksq, view);
        else
            nnue.move(*this, prev, dpc.pc, dpc.from, dpc.to, ksq, view);
    }

    assert(is_initialized(view));
}

// AccumTable

void AccumTable::reset() {
    for(Color c : {WHITE, BLACK})
        for(int i = 0; i < 2 * BUCKET_SIZE; i++)
            entries[c][i].reset();
}

void AccumTable::refresh(Board &board, Color view) {
    assert(valid_color(view));

    const Square ksq = board.king_sq(view);
    const int ksq_idx = KING_BUCKET[rel_sq(view, ksq)];
    AccumEntry &entry = entries[view][(sq_file(ksq) > 3) * BUCKET_SIZE + ksq_idx];

    Accum &entry_acc = entry.get_accum();

    for(Color c : {WHITE, BLACK}) {
        for(int i = PAWN; i <= KING; i++) {
            PieceType pt = PieceType(i);
            Piece pc = make_piece(c, pt);

            const U64 pc_bb = board.get_piecebb(c, pt);
            const U64 entry_bb = entry.get_piecebb(c, pt);

            U64 to_set = pc_bb & ~entry_bb;
            while(to_set)
                nnue.put(entry_acc, entry_acc, pc, pop_lsb(to_set), ksq, view);

            U64 to_clear = entry_bb & ~pc_bb;
            while(to_clear)
                nnue.remove(entry_acc, entry_acc, pc, pop_lsb(to_clear), ksq, view);

            entry.set_piecebb(c, pt, pc_bb);
        }
    }

    Accum &acc = board.get_accum();
    acc.mark_as_initialized(view);

    memcpy(acc.get_data(view), entry_acc.get_data(view), sizeof(int16_t) * FT_SIZE);
}

} // namespace NNUE
