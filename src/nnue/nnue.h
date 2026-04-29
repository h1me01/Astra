#pragma once

#include <cassert>
#include <cstring>
#include <utility>

#include "../chess/types.h"
#include "accumulator.h"
#include "aligned_array.h"
#include "arch.h"
#include "simd.h"

namespace astra {
class Board;
}

namespace astra::nnue {

inline bool needs_refresh(Piece pc, Square from, Square to, Color view) {
    assert(valid_piece(pc));
    assert(valid_color(view));

    if (piece_type(pc) != KING || piece_color(pc) != view)
        return false;

    assert(valid_sq(from));
    assert(valid_sq(to));

    return INPUT_BUCKET(rel_sq(view, from)) != INPUT_BUCKET(rel_sq(view, to)) ||
           (sq_file(from) > FILE_D) != (sq_file(to) > FILE_D);
}

class NNUE {
    using NNZOutput = std::pair<int, AlignedArray<uint16_t, FT_SIZE / 4>>;

  public:
    void init();

    void init_accum(Accumulator& acc) const {
        for (Color c : {WHITE, BLACK})
            std::memcpy(acc.data[c], &ft_bias_, sizeof(int16_t) * FT_SIZE);
    }

    int16_t* feature_weight(Piece pc, Square psq, Square ksq, Color view) {
        assert(valid_sq(psq));
        assert(valid_piece(pc));

        // mirror psq horizontally if king is on other half
        if (sq_file(ksq) > FILE_D)
            psq = static_cast<Square>(psq ^ 7);

        int idx = rel_sq(view, psq)                    //
                  + piece_type(pc) * 64                //
                  + (piece_color(pc) != view) * 6 * 64 //
                  + INPUT_BUCKET(rel_sq(view, ksq)) * 768;

        return &ft_weight_[idx * FT_SIZE];
    }

    int32_t forward(Board& board, const Accumulator& acc);

  private:
    AlignedArray<int16_t, INPUT_SIZE * FT_SIZE> ft_weight_;
    AlignedArray<int16_t, FT_SIZE> ft_bias_;
    AlignedArray<int8_t, OUTPUT_BUCKETS, FT_SIZE * L1_SIZE> l1_weight_;
    AlignedArray<float, OUTPUT_BUCKETS, L1_SIZE> l1_bias_;
    AlignedArray<float, OUTPUT_BUCKETS, 2 * L1_SIZE * L2_SIZE> l2_weight_;
    AlignedArray<float, OUTPUT_BUCKETS, L2_SIZE> l2_bias_;
    AlignedArray<float, OUTPUT_BUCKETS, L2_SIZE> l3_weight_;
    AlignedArray<float, OUTPUT_BUCKETS> l3_bias_;
    AlignedArray<uint16_t, 256, 8> nnz_lookup_;

    AlignedArray<uint8_t, FT_SIZE> prep_l1_input(const Color stm, const Accumulator& acc);
    NNZOutput find_nnz(const AlignedArray<uint8_t, FT_SIZE>& input);
    AlignedArray<float, 2 * L1_SIZE> forward_l1(int bucket, const AlignedArray<uint8_t, FT_SIZE>& input);
    AlignedArray<float, L2_SIZE> forward_l2(int bucket, const AlignedArray<float, 2 * L1_SIZE>& input);
    float forward_l3(int bucket, const AlignedArray<float, L2_SIZE>& input);
};

// Global Variable

extern NNUE nnue;

} // namespace astra::nnue
