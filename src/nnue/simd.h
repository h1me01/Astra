#pragma once

#include <cstdint>
#include <immintrin.h>

#if defined(__AVX512F__)

#define USE_SIMD
#define VEC_PREFIX(x) _mm512_##x

using vec_type = __m512i;

constexpr int VEC_DIV = 32;
constexpr int ALIGNMENT = 64;

#elif defined(__AVX2__) || defined(__AVX__)

#define USE_SIMD
#define VEC_PREFIX(x) _mm256_##x

using vec_type = __m256i;

constexpr int VEC_DIV = 16;
constexpr int ALIGNMENT = 32;

#else

using vec_type = int16_t;

constexpr int VEC_DIV = 1;
constexpr int ALIGNMENT = 32;

#endif

#ifdef USE_SIMD

#define madd_epi16 VEC_PREFIX(madd_epi16)
#define add_epi32 VEC_PREFIX(add_epi32)
#define add_epi16 VEC_PREFIX(add_epi16)
#define sub_epi16 VEC_PREFIX(sub_epi16)
#define max_epi16 VEC_PREFIX(max_epi16)
#define min_epi16 VEC_PREFIX(min_epi16)
#define set1_epi16 VEC_PREFIX(set1_epi16)
#define mullo_epi16 VEC_PREFIX(mullo_epi16)

#define VEC_ADD(a, b) add_epi16(a, b)
#define VEC_SUB(a, b) sub_epi16(a, b)

#else

#define VEC_ADD(a, b) ((a) + (b))
#define VEC_SUB(a, b) ((a) - (b))

#endif

namespace SIMD {

inline int32_t hor_sum([[maybe_unused]] const vec_type &reg) {
#if defined(__AVX512F__)
    return _mm512_reduce_add_epi32(reg);
#elif defined(__AVX2__) || defined(__AVX__)
    const __m128i low = _mm256_castsi256_si128(reg);
    const __m128i high = _mm256_extracti128_si256(reg, 1);
    const __m128i sum128 = _mm_add_epi32(low, high);

    const __m128i sum64 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
    const __m128i sum32 = _mm_add_epi32(sum64, _mm_srli_si128(sum64, 4));

    return _mm_cvtsi128_si32(sum32);
#else
    return 0;
#endif
}

} // namespace SIMD
