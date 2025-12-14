#pragma once

#include <cstdint>
#include <immintrin.h>

#include "constants.h"

#if defined(__AVX512F__)

#define USE_SIMD
#define VEC_PREFIX(x) _mm512_##x

using ivec_t = __m512i;
using fvec_t = __m512;

#elif defined(__AVX2__) || defined(__AVX__)

#define USE_SIMD
#define VEC_PREFIX(x) _mm256_##x

using ivec_t = __m256i;
using fvec_t = __m256;

#else

using ivec_t = int16_t;
using fvec_t = float;

#endif

#ifdef USE_SIMD

#define madd_epi16 VEC_PREFIX(madd_epi16)
#define add_epi32 VEC_PREFIX(add_epi32)
#define add_epi16 VEC_PREFIX(add_epi16)
#define sub_epi16 VEC_PREFIX(sub_epi16)
#define max_epi16 VEC_PREFIX(max_epi16)
#define min_epi16 VEC_PREFIX(min_epi16)
#define set1_epi16 VEC_PREFIX(set1_epi16)
#define set1_epi32 VEC_PREFIX(set1_epi32)
#define set1_ps VEC_PREFIX(set1_ps)
#define max_ps VEC_PREFIX(max_ps)
#define min_ps VEC_PREFIX(min_ps)
#define slli_epi16 VEC_PREFIX(slli_epi16)
#define mulhi_epi16 VEC_PREFIX(mulhi_epi16)
#define packus_epi16 VEC_PREFIX(packus_epi16)
#define maddubs_epi16 VEC_PREFIX(maddubs_epi16)
#define mul_ps VEC_PREFIX(mul_ps)
#define add_ps VEC_PREFIX(add_ps)
#define cvtepi32_ps VEC_PREFIX(cvtepi32_ps)
#define fmadd_ps VEC_PREFIX(fmadd_ps)

#if defined(__AVX512F__)
#define store_si _mm512_store_si512

const ivec_t ZERO_IVEC = _mm512_setzero_si512();
const fvec_t ZERO_FVEC = _mm512_setzero_ps();

#else
#define store_si _mm256_store_si256

const ivec_t ZERO_IVEC = _mm256_setzero_si256();
const fvec_t ZERO_FVEC = _mm256_setzero_ps();

#endif

#define ACCUM_ADD(a, b) add_epi16(a, b)
#define ACCUM_SUB(a, b) sub_epi16(a, b)

constexpr int FLOAT_VEC_SIZE = sizeof(fvec_t) / sizeof(float);
constexpr int INT32_VEC_SIZE = sizeof(ivec_t) / sizeof(int32_t);
constexpr int INT16_VEC_SIZE = sizeof(ivec_t) / sizeof(int16_t);

const ivec_t FT_QUANT_IVEC = set1_epi16(NNUE::FT_QUANT);
const fvec_t ONE_FVEC = set1_ps(1.0f);

const fvec_t DEQUANT_MULT_PS = set1_ps(NNUE::DEQUANT_MULT);

#else

#define ACCUM_ADD(a, b) ((a) + (b))
#define ACCUM_SUB(a, b) ((a) - (b))

constexpr int INT16_VEC_SIZE = 1; // define this so that non simd code works

#endif

namespace SIMD {

#ifdef USE_SIMD

inline ivec_t clamp_epi16(ivec_t val, ivec_t min, ivec_t max) {
    return min_epi16(max_epi16(val, min), max);
}

inline fvec_t clamp_ps(fvec_t val, fvec_t min, fvec_t max) {
    return min_ps(max_ps(val, min), max);
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

    return ((float *) &sum64)[0] + ((float *) &sum64)[1];
#endif
}

#if defined(__AVX512F__) && defined(__AVX512BW__)

inline uint32_t nnz_nonzero_mask(ivec_t v) {
    return _mm512_cmpgt_epi32_mask(v, ZERO_IVEC);
}

#if defined(__AVX512VNNI__)

inline ivec_t dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b) {
    return _mm512_dpbusd_epi32(sum, a, b);
}

inline ivec_t double_dpbusd_epi32(ivec_t sum, ivec_t a1, ivec_t a2, ivec_t a3, ivec_t a4) {
    return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, a1, a2), a3, a4);
}

#endif

#else

inline uint32_t nnz_nonzero_mask(ivec_t v) {
    return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(v, ZERO_IVEC)));
}

#endif

#if !defined(__AVX512VNNI__)

inline ivec_t dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b) {
    ivec_t sum32 = madd_epi16(maddubs_epi16(a, b), set1_epi16(1));
    return add_epi32(sum32, sum);
}

inline ivec_t double_dpbusd_epi32(ivec_t sum, ivec_t a1, ivec_t a2, ivec_t a3, ivec_t a4) {
    ivec_t mul1 = maddubs_epi16(a1, a2);
    ivec_t mul2 = maddubs_epi16(a3, a4);
    ivec_t sum32 = madd_epi16(add_epi16(mul1, mul2), set1_epi16(1));
    return add_epi32(sum32, sum);
}

#endif

#endif

} // namespace SIMD
