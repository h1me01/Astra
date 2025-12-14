#pragma once

#include "simd.h"

namespace nnue {

#ifdef USE_SIMD
void permute_simd_data(__m128i *vec, int count) {
    count /= (sizeof(__m128i) / sizeof(int16_t));

#if (defined(__AVX512F__) && defined(__AVX512BW__))
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
#endif

template <typename To, typename From> //
inline To *ptr_cast(From *ptr) {
    return reinterpret_cast<To *>(ptr);
}

template <typename To, typename T, size_t N> //
inline const To *ptr_cast(const LayerOutput<T, N> &output) {
    return reinterpret_cast<const To *>(static_cast<const T *>(output));
}

template <typename To, typename T, size_t N> //
inline To *ptr_cast(LayerOutput<T, N> &output) {
    return reinterpret_cast<To *>(static_cast<T *>(output));
}

#define DEFINE_VEC_AT(vec_type, func_name)                                                                             \
    template <typename T> inline vec_type &func_name(T *ptr, size_t offset = 0) {                                      \
        return *ptr_cast<vec_type>(ptr + offset);                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T> inline const vec_type &func_name(const T *ptr, size_t offset = 0) {                          \
        return *ptr_cast<const vec_type>(ptr + offset);                                                                \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T, size_t N> inline vec_type &func_name(LayerOutput<T, N> &output, size_t offset = 0) {         \
        return *ptr_cast<vec_type>(ptr_cast<T>(output) + offset);                                                      \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T, size_t N>                                                                                    \
    inline const vec_type &func_name(const LayerOutput<T, N> &output, size_t offset = 0) {                             \
        return *ptr_cast<const vec_type>(ptr_cast<const T>(output) + offset);                                          \
    }

DEFINE_VEC_AT(ivec_t, ivec_at)
DEFINE_VEC_AT(fvec_t, fvec_at)

#undef DEFINE_VEC_AT

template <typename T> //
void transpose(T *weights, int rows, int cols) {
    T transposed[cols * rows];
    for(int i = 0; i < cols; i++)
        for(int j = 0; j < rows; j++)
            transposed[i * rows + j] = weights[j * cols + i];
    memcpy(weights, transposed, sizeof(T) * cols * rows);
}

} // namespace nnue
