#pragma once

#include <cassert>
#include <cstring>
#include <utility>

#include "../chess/types.h"
#include "accumulator.h"
#include "arch.h"
#include "simd.h"
#include "util.h"

namespace astra {
class Board;
} // namespace astra

namespace astra::nnue {

class NNUE {
    using NNZOutput = std::pair<int, NDArray<uint16_t, FT_SIZE / 4>>;

  public:
    void init();

    void init_accum(Accumulator& acc) const {
        for (Color c : {WHITE, BLACK})
            std::memcpy(&acc.data(c, 0), &ft_bias_, sizeof(int16_t) * FT_SIZE);
    }

    int16_t* feature_weight(Piece pc, Square psq, Square ksq, Color view) {
        assert(is_valid(psq));
        assert(is_valid(pc));

        // mirror psq horizontally if king is on other half
        if (file_of(ksq) > FILE_D)
            psq = static_cast<Square>(psq ^ 7);

        int idx = relative_sq(view, psq)            //
                  + type_of(pc) * 64                //
                  + (color_of(pc) != view) * 6 * 64 //
                  + INPUT_BUCKET(relative_sq(view, ksq)) * 768;

        return &ft_weight_(idx * FT_SIZE);
    }

    int32_t forward(Board& board, const Accumulator& acc);

  private:
    alignas(64) NDArray<int16_t, INPUT_SIZE * FT_SIZE> ft_weight_;
    alignas(64) NDArray<int16_t, FT_SIZE> ft_bias_;
    alignas(64) NDArray<int8_t, OUTPUT_BUCKETS, FT_SIZE * L1_SIZE> l1_weight_;
    alignas(64) NDArray<float, OUTPUT_BUCKETS, L1_SIZE> l1_bias_;
    alignas(64) NDArray<float, OUTPUT_BUCKETS, 2 * L1_SIZE * L2_SIZE> l2_weight_;
    alignas(64) NDArray<float, OUTPUT_BUCKETS, L2_SIZE> l2_bias_;
    alignas(64) NDArray<float, OUTPUT_BUCKETS, L2_SIZE> l3_weight_;
    alignas(64) NDArray<float, OUTPUT_BUCKETS> l3_bias_;
    alignas(64) NDArray<uint16_t, 256, 8> nnz_lookup_;

    NDArray<uint8_t, FT_SIZE> prep_l1_input(const Color stm, const Accumulator& acc);
    NNZOutput find_nnz(const NDArray<uint8_t, FT_SIZE>& input);
    NDArray<float, 2 * L1_SIZE> forward_l1(int bucket, const NDArray<uint8_t, FT_SIZE>& input);
    NDArray<float, L2_SIZE> forward_l2(int bucket, const NDArray<float, 2 * L1_SIZE>& input);
    float forward_l3(int bucket, const NDArray<float, L2_SIZE>& input);
};

// Global Variable

extern NNUE nnue;

} // namespace astra::nnue
