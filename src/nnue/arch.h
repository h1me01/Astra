#pragma once

#include "../ndarray.h"

namespace astra::nnue {

constexpr NDArray<int, 64> INPUT_BUCKET = {
    0, 1, 2, 3, 3, 2, 1, 0, //
    4, 5, 6, 7, 7, 6, 5, 4, //
    8, 8, 8, 8, 8, 8, 8, 8, //
    9, 9, 9, 9, 9, 9, 9, 9, //
    9, 9, 9, 9, 9, 9, 9, 9, //
    9, 9, 9, 9, 9, 9, 9, 9, //
    9, 9, 9, 9, 9, 9, 9, 9, //
    9, 9, 9, 9, 9, 9, 9, 9, //
};

constexpr int INPUT_BUCKETS = INPUT_BUCKET.max() + 1;

constexpr int INPUT_SIZE = INPUT_BUCKETS * 768;
constexpr int FT_SIZE = 1536;
constexpr int L1_SIZE = 16;
constexpr int L2_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int EVAL_SCALE = 400;

} // namespace astra::nnue
