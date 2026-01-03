#pragma once

#include <cstdint>
#include <immintrin.h>

#include "constants.h"

// clang-format off

#if defined(__AVX512F__)
    #define SIMD_OP(x) _mm512_##x
    using ivec_t = __m512i;
    using fvec_t = __m512;
#elif defined(__AVX2__) || defined(__AVX__)
    #define SIMD_OP(x) _mm256_##x
    using ivec_t = __m256i;
    using fvec_t = __m256;
#endif

// integer operations
#define madd_epi16    SIMD_OP(madd_epi16)
#define add_epi32     SIMD_OP(add_epi32)
#define add_epi16     SIMD_OP(add_epi16)
#define sub_epi16     SIMD_OP(sub_epi16)
#define max_epi16     SIMD_OP(max_epi16)
#define min_epi16     SIMD_OP(min_epi16)
#define set1_epi16    SIMD_OP(set1_epi16)
#define set1_epi32    SIMD_OP(set1_epi32)
#define slli_epi16    SIMD_OP(slli_epi16)
#define mulhi_epi16   SIMD_OP(mulhi_epi16)
#define packus_epi16  SIMD_OP(packus_epi16)
#define maddubs_epi16 SIMD_OP(maddubs_epi16)

// float operations
#define set1_ps       SIMD_OP(set1_ps)
#define max_ps        SIMD_OP(max_ps)
#define min_ps        SIMD_OP(min_ps)
#define mul_ps        SIMD_OP(mul_ps)
#define add_ps        SIMD_OP(add_ps)
#define cvtepi32_ps   SIMD_OP(cvtepi32_ps)
#define fmadd_ps      SIMD_OP(fmadd_ps)

// clang-format on

namespace simd {

// constants

constexpr int FLOAT_VEC_SIZE = sizeof(fvec_t) / sizeof(float);
constexpr int INT32_VEC_SIZE = sizeof(ivec_t) / sizeof(int32_t);
constexpr int INT16_VEC_SIZE = sizeof(ivec_t) / sizeof(int16_t);

const ivec_t FT_QUANT_IVEC = set1_epi16(nnue::FT_QUANT);
const fvec_t DEQUANT_MULT_PS = set1_ps(nnue::DEQUANT_MULT);
const fvec_t ONE_FVEC = set1_ps(1.0f);

#if defined(__AVX512F__)
const ivec_t ZERO_IVEC = _mm512_setzero_si512();
const fvec_t ZERO_FVEC = _mm512_setzero_ps();
#else
const ivec_t ZERO_IVEC = _mm256_setzero_si256();
const fvec_t ZERO_FVEC = _mm256_setzero_ps();
#endif

// utility functions

inline ivec_t clamp_epi16(ivec_t val, ivec_t min_val, ivec_t max_val) {
    return min_epi16(max_epi16(val, min_val), max_val);
}

inline fvec_t clamp_ps(fvec_t val, fvec_t min_val, fvec_t max_val) {
    return min_ps(max_ps(val, min_val), max_val);
}

inline float hor_sum_ps(fvec_t *v) {
#if defined(__AVX512F__)
    return _mm512_reduce_add_ps(v[0]);
#else
    v[0] = add_ps(v[0], v[1]);
    __m128 high = _mm256_extractf128_ps(v[0], 1);
    __m128 low = _mm256_castps256_ps128(v[0]);
    __m128 sum = _mm_add_ps(high, low);
    __m128 high64 = _mm_movehl_ps(sum, sum);
    __m128 sum64 = _mm_add_ps(sum, high64);
    return reinterpret_cast<float *>(&sum64)[0] + reinterpret_cast<float *>(&sum64)[1];
#endif
}

inline uint32_t nnz_non_zero_mask(ivec_t v) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
    return _mm512_cmpgt_epi32_mask(v, ZERO_IVEC);
#else
    return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(v, ZERO_IVEC)));
#endif
}

#if defined(__AVX512VNNI__)
inline ivec_t dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b) {
    return _mm512_dpbusd_epi32(sum, a, b);
}

inline ivec_t double_dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b, ivec_t c, ivec_t d) {
    return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, a, b), c, d);
}
#else
inline ivec_t dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b) {
    auto product = maddubs_epi16(a, b);
    auto sum32 = madd_epi16(product, set1_epi16(1));
    return add_epi32(sum, sum32);
}

inline ivec_t double_dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b, ivec_t c, ivec_t d) {
    auto mul1 = maddubs_epi16(a, b);
    auto mul2 = maddubs_epi16(c, d);
    auto sum32 = madd_epi16(add_epi16(mul1, mul2), set1_epi16(1));
    return add_epi32(sum, sum32);
}
#endif

inline void permute_simd_data(__m128i *vec, int count) {
    count /= (sizeof(__m128i) / sizeof(int16_t));

#if(defined(__AVX512F__) && defined(__AVX512BW__))
    const int PACKUS_BLOCKS = 8;
    const int PERMUTATION[PACKUS_BLOCKS] = {0, 2, 4, 6, 1, 3, 5, 7};
#else
    const int PACKUS_BLOCKS = 4;
    const int PERMUTATION[PACKUS_BLOCKS] = {0, 2, 1, 3};
#endif

    __m128i regs[PACKUS_BLOCKS];
    for(int i = 0; i < count; i += PACKUS_BLOCKS) {
        for(int j = 0; j < PACKUS_BLOCKS; j++)
            regs[j] = vec[i + j];
        for(int j = 0; j < PACKUS_BLOCKS; j++)
            vec[i + j] = regs[PERMUTATION[j]];
    }
}

} // namespace simd
