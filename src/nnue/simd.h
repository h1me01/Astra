#pragma once

#include <cstdint>
#include <immintrin.h>

#include "arch.h"

namespace astra::simd {

// Types

#if defined(__AVX512F__)
using ivec_t = __m512i;
using fvec_t = __m512;
#elif defined(__AVX2__) || defined(__AVX__)
using ivec_t = __m256i;
using fvec_t = __m256;
#endif

// Integer operations

#if defined(__AVX512F__)
inline ivec_t madd_epi16(ivec_t a, ivec_t b) { return _mm512_madd_epi16(a, b); }
inline ivec_t add_epi32(ivec_t a, ivec_t b) { return _mm512_add_epi32(a, b); }
inline ivec_t add_epi16(ivec_t a, ivec_t b) { return _mm512_add_epi16(a, b); }
inline ivec_t sub_epi16(ivec_t a, ivec_t b) { return _mm512_sub_epi16(a, b); }
inline ivec_t max_epi16(ivec_t a, ivec_t b) { return _mm512_max_epi16(a, b); }
inline ivec_t min_epi16(ivec_t a, ivec_t b) { return _mm512_min_epi16(a, b); }
inline ivec_t set1_epi16(int16_t a) { return _mm512_set1_epi16(a); }
inline ivec_t set1_epi32(int32_t a) { return _mm512_set1_epi32(a); }
inline ivec_t slli_epi16(ivec_t a, int imm) { return _mm512_slli_epi16(a, imm); }
inline ivec_t mulhi_epi16(ivec_t a, ivec_t b) { return _mm512_mulhi_epi16(a, b); }
inline ivec_t packus_epi16(ivec_t a, ivec_t b) { return _mm512_packus_epi16(a, b); }
inline ivec_t maddubs_epi16(ivec_t a, ivec_t b) { return _mm512_maddubs_epi16(a, b); }
inline ivec_t zero_ivec() { return _mm512_setzero_si512(); }
#else
inline ivec_t madd_epi16(ivec_t a, ivec_t b) { return _mm256_madd_epi16(a, b); }
inline ivec_t add_epi32(ivec_t a, ivec_t b) { return _mm256_add_epi32(a, b); }
inline ivec_t add_epi16(ivec_t a, ivec_t b) { return _mm256_add_epi16(a, b); }
inline ivec_t sub_epi16(ivec_t a, ivec_t b) { return _mm256_sub_epi16(a, b); }
inline ivec_t max_epi16(ivec_t a, ivec_t b) { return _mm256_max_epi16(a, b); }
inline ivec_t min_epi16(ivec_t a, ivec_t b) { return _mm256_min_epi16(a, b); }
inline ivec_t set1_epi16(int16_t a) { return _mm256_set1_epi16(a); }
inline ivec_t set1_epi32(int32_t a) { return _mm256_set1_epi32(a); }
inline ivec_t slli_epi16(ivec_t a, int imm) { return _mm256_slli_epi16(a, imm); }
inline ivec_t mulhi_epi16(ivec_t a, ivec_t b) { return _mm256_mulhi_epi16(a, b); }
inline ivec_t packus_epi16(ivec_t a, ivec_t b) { return _mm256_packus_epi16(a, b); }
inline ivec_t maddubs_epi16(ivec_t a, ivec_t b) { return _mm256_maddubs_epi16(a, b); }
inline ivec_t zero_ivec() { return _mm256_setzero_si256(); }
#endif

// Float operations

#if defined(__AVX512F__)
inline fvec_t set1_ps(float a) { return _mm512_set1_ps(a); }
inline fvec_t max_ps(fvec_t a, fvec_t b) { return _mm512_max_ps(a, b); }
inline fvec_t min_ps(fvec_t a, fvec_t b) { return _mm512_min_ps(a, b); }
inline fvec_t mul_ps(fvec_t a, fvec_t b) { return _mm512_mul_ps(a, b); }
inline fvec_t add_ps(fvec_t a, fvec_t b) { return _mm512_add_ps(a, b); }
inline fvec_t cvtepi32_ps(ivec_t a) { return _mm512_cvtepi32_ps(a); }
inline fvec_t fmadd_ps(fvec_t a, fvec_t b, fvec_t c) { return _mm512_fmadd_ps(a, b, c); }
inline fvec_t zero_fvec() { return _mm512_setzero_ps(); }
#else
inline fvec_t set1_ps(float a) { return _mm256_set1_ps(a); }
inline fvec_t max_ps(fvec_t a, fvec_t b) { return _mm256_max_ps(a, b); }
inline fvec_t min_ps(fvec_t a, fvec_t b) { return _mm256_min_ps(a, b); }
inline fvec_t mul_ps(fvec_t a, fvec_t b) { return _mm256_mul_ps(a, b); }
inline fvec_t add_ps(fvec_t a, fvec_t b) { return _mm256_add_ps(a, b); }
inline fvec_t cvtepi32_ps(ivec_t a) { return _mm256_cvtepi32_ps(a); }
inline fvec_t fmadd_ps(fvec_t a, fvec_t b, fvec_t c) { return _mm256_fmadd_ps(a, b, c); }
inline fvec_t zero_fvec() { return _mm256_setzero_ps(); }
#endif

// Constants

constexpr int FLOAT_VEC_SIZE = sizeof(fvec_t) / sizeof(float);
constexpr int INT32_VEC_SIZE = sizeof(ivec_t) / sizeof(int32_t);
constexpr int INT16_VEC_SIZE = sizeof(ivec_t) / sizeof(int16_t);

// Utility Functions

inline ivec_t clamp_epi16(ivec_t val, ivec_t min_val, ivec_t max_val) {
    return min_epi16(max_epi16(val, min_val), max_val);
}

inline fvec_t clamp_ps(fvec_t val, fvec_t min_val, fvec_t max_val) { return min_ps(max_ps(val, min_val), max_val); }

inline float hor_sum_ps(fvec_t* v) {
#if defined(__AVX512F__)
    return _mm512_reduce_add_ps(v[0]);
#else
    v[0] = add_ps(v[0], v[1]);
    __m128 high = _mm256_extractf128_ps(v[0], 1);
    __m128 low = _mm256_castps256_ps128(v[0]);
    __m128 sum = _mm_add_ps(high, low);
    __m128 hi64 = _mm_movehl_ps(sum, sum);
    __m128 sum64 = _mm_add_ps(sum, hi64);
    auto* f = reinterpret_cast<float*>(&sum64);
    return f[0] + f[1];
#endif
}

inline uint32_t nnz_non_zero_mask(ivec_t v) {
#if defined(__AVX512F__) && defined(__AVX512BW__)
    return _mm512_cmpgt_epi32_mask(v, zero_ivec());
#else
    return _mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(v, zero_ivec())));
#endif
}

inline ivec_t dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b) {
#if defined(__AVX512VNNI__)
    return _mm512_dpbusd_epi32(sum, a, b);
#else
    return add_epi32(sum, madd_epi16(maddubs_epi16(a, b), set1_epi16(1)));
#endif
}

inline ivec_t double_dpbusd_epi32(ivec_t sum, ivec_t a, ivec_t b, ivec_t c, ivec_t d) {
#if defined(__AVX512VNNI__)
    return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(sum, a, b), c, d);
#else
    return add_epi32(sum, madd_epi16(add_epi16(maddubs_epi16(a, b), maddubs_epi16(c, d)), set1_epi16(1)));
#endif
}

inline void permute_simd_data(__m128i* vec, int count) {
    count /= (sizeof(__m128i) / sizeof(int16_t));

#if (defined(__AVX512F__) && defined(__AVX512BW__))
    const int packus_blocks = 8;
    const int perm[packus_blocks] = {0, 2, 4, 6, 1, 3, 5, 7};
#else
    const int packus_blocks = 4;
    const int perm[packus_blocks] = {0, 2, 1, 3};
#endif

    __m128i regs[packus_blocks];
    for (int i = 0; i < count; i += packus_blocks) {
        for (int j = 0; j < packus_blocks; ++j)
            regs[j] = vec[i + j];
        for (int j = 0; j < packus_blocks; ++j)
            vec[i + j] = regs[perm[j]];
    }
}

} // namespace astra::simd
