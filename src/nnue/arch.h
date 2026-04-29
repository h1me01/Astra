#pragma once

#include <algorithm>
#include <array>

#include "../ndarray.h"

namespace astra::nnue {

constexpr NDArray<int, 64> INPUT_BUCKET = {
    0, 1, 2, 3, 3, 2, 1, 0, //
    4, 4, 5, 5, 5, 5, 4, 4, //
    6, 6, 6, 6, 6, 6, 6, 6, //
    6, 6, 6, 6, 6, 6, 6, 6, //
    6, 6, 6, 6, 6, 6, 6, 6, //
    6, 6, 6, 6, 6, 6, 6, 6, //
    6, 6, 6, 6, 6, 6, 6, 6, //
    6, 6, 6, 6, 6, 6, 6, 6, //
};

constexpr int INPUT_BUCKETS = INPUT_BUCKET.max() + 1;

constexpr int INPUT_SIZE = INPUT_BUCKETS * 768;
constexpr int FT_SIZE = 1024;
constexpr int L1_SIZE = 16;
constexpr int L2_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int EVAL_SCALE = 400;

} // namespace astra::nnue
