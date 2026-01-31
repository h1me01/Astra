#pragma once

namespace nnue {

constexpr int INPUT_BUCKETS = 1;
constexpr int FEATURE_SIZE = 768;

constexpr int INPUT_SIZE = INPUT_BUCKETS * FEATURE_SIZE;
constexpr int FT_SIZE = 128;
constexpr int L1_SIZE = 16;
constexpr int L2_SIZE = 32;
constexpr int OUTPUT_BUCKETS = 8;

constexpr int FT_SHIFT = 9;
constexpr int FT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int EVAL_SCALE = 400;

constexpr int ALIGNMENT = 64;
constexpr int INT8_PER_INT32 = sizeof(int32_t) / sizeof(int8_t);

constexpr int INPUT_BUCKET[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 0, 0, 0, 0, 0, 0, 0, //
};

constexpr float DEQUANT_MULT = (1 << FT_SHIFT) / static_cast<float>(FT_QUANT * FT_QUANT * L1_QUANT);

} // namespace nnue
