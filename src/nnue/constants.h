#pragma once

namespace nnue {

constexpr int INPUT_BUCKETS = 13;
constexpr int FEATURE_SIZE = 768;

constexpr int INPUT_SIZE = INPUT_BUCKETS * FEATURE_SIZE;
constexpr int FT_SIZE = 1536;
constexpr int L1_SIZE = 16;
constexpr int L2_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_SHIFT = 9;
constexpr int FT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

constexpr int EVAL_SCALE = 400;

constexpr float DEQUANT_MULT = static_cast<float>(1 << FT_SHIFT) / static_cast<float>(FT_QUANT * FT_QUANT * L1_QUANT);

constexpr int INPUT_BUCKET[64] = {
    0,  1,  2,  3,  3,  2,  1,  0,  //
    4,  5,  6,  7,  7,  6,  5,  4,  //
    8,  8,  9,  9,  9,  9,  8,  8,  //
    10, 10, 10, 10, 10, 10, 10, 10, //
    11, 11, 11, 11, 11, 11, 11, 11, //
    11, 11, 11, 11, 11, 11, 11, 11, //
    12, 12, 12, 12, 12, 12, 12, 12, //
    12, 12, 12, 12, 12, 12, 12, 12, //
};

} // namespace nnue
