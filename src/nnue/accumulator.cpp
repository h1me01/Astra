#include <cstring>

#include "../chess/board.h"
#include "accumulator.h"
#include "nnue.h"

namespace astra::nnue {

void Accumulator::update(const Accumulator& src, Color view) {
    assert(is_valid(view));

    for (const auto& dp : dirty_pieces) {
        if (dp.is_move())
            move(src, dp.pc, dp.from, dp.to, king_sq(view), view);
        else if (dp.is_add())
            put(src, dp.pc, dp.to, king_sq(view), view);
        else
            remove(src, dp.pc, dp.from, king_sq(view), view);
    }

    assert(initialized(view));
}

void Accumulator::put(const Accumulator& src, Piece pc, Square psq, Square ksq, Color view) {
    const auto w = ptr_cast<const simd::ivec_t>(nnue.feature_weight(pc, psq, ksq, view));

    auto* dst_vec = ptr_cast<simd::ivec_t>(&data(view, 0));
    const auto* src_vec = initialized(view) ? dst_vec : ptr_cast<const simd::ivec_t>(&src.data(view, 0));

    for (int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; ++i)
        dst_vec[i] = simd::add_epi16(src_vec[i], w[i]);

    initialized(view) = true;
}

void Accumulator::remove(const Accumulator& src, Piece pc, Square psq, Square ksq, Color view) {
    const auto w = ptr_cast<const simd::ivec_t>(nnue.feature_weight(pc, psq, ksq, view));

    auto* dst_vec = ptr_cast<simd::ivec_t>(&data(view, 0));
    const auto* src_vec = initialized(view) ? dst_vec : ptr_cast<const simd::ivec_t>(&src.data(view, 0));

    for (int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; ++i)
        dst_vec[i] = simd::sub_epi16(src_vec[i], w[i]);

    initialized(view) = true;
}

void Accumulator::move(const Accumulator& src, Piece pc, Square from, Square to, Square ksq, Color view) {
    const auto wf = ptr_cast<const simd::ivec_t>(nnue.feature_weight(pc, from, ksq, view));
    const auto wt = ptr_cast<const simd::ivec_t>(nnue.feature_weight(pc, to, ksq, view));

    auto* dst_vec = ptr_cast<simd::ivec_t>(&data(view, 0));
    const auto* src_vec = initialized(view) ? dst_vec : ptr_cast<const simd::ivec_t>(&src.data(view, 0));

    for (int i = 0; i < FT_SIZE / simd::INT16_VEC_SIZE; ++i)
        dst_vec[i] = simd::add_epi16(src_vec[i], simd::sub_epi16(wt[i], wf[i]));

    initialized(view) = true;
}

void AccumulatorEntry::reset() {
    pieces_bb.fill(0);
    nnue.init_accum(accum);
}

void AccumulatorList::refresh(Color view, Board& board) {
    assert(is_valid(view));

    const Square ksq = board.king_sq(view);
    const int ksq_idx = INPUT_BUCKET(relative_sq(view, ksq));
    auto& entry = entries_(view, (file_of(ksq) > FILE_D) * INPUT_BUCKETS + ksq_idx);

    for (Color c : {WHITE, BLACK}) {
        for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            Piece pc = make_piece(c, pt);
            Bitboard pc_bb = board.piece_bb(c, pt);
            Bitboard entry_bb = entry.pieces_bb(c, pt);

            for (Bitboard bb = pc_bb & ~entry_bb; bb;)
                entry.accum.put(entry.accum, pc, pop_lsb(bb), ksq, view);
            for (Bitboard bb = entry_bb & ~pc_bb; bb;)
                entry.accum.remove(entry.accum, pc, pop_lsb(bb), ksq, view);

            entry.pieces_bb(c, pt) = pc_bb;
        }
    }

    auto& accum = back();
    std::memcpy(&accum.data(view, 0), &entry.accum.data(view, 0), sizeof(int16_t) * FT_SIZE);
    accum.initialized(view) = true;
}

} // namespace astra::nnue
