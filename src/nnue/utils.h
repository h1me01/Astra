#pragma once

#include "simd.h"

namespace nnue {

template <typename To, typename From>
inline To* ptr_cast(From* ptr) {
    return reinterpret_cast<To*>(ptr);
}

template <typename To, typename T, size_t N>
inline const To* ptr_cast(const LayerOutput<T, N>& output) {
    return reinterpret_cast<const To*>(static_cast<const T*>(output));
}

template <typename To, typename T, size_t N>
inline To* ptr_cast(LayerOutput<T, N>& output) {
    return reinterpret_cast<To*>(static_cast<T*>(output));
}

#define DEFINE_VEC_AT(vec_type, func_name)                                                                             \
    template <typename T>                                                                                              \
    inline vec_type& func_name(T* ptr, size_t offset = 0) {                                                            \
        return *ptr_cast<vec_type>(ptr + offset);                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T>                                                                                              \
    inline const vec_type& func_name(const T* ptr, size_t offset = 0) {                                                \
        return *ptr_cast<const vec_type>(ptr + offset);                                                                \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T, size_t N>                                                                                    \
    inline vec_type& func_name(LayerOutput<T, N>& output, size_t offset = 0) {                                         \
        return *ptr_cast<vec_type>(ptr_cast<T>(output) + offset);                                                      \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T, size_t N>                                                                                    \
    inline const vec_type& func_name(const LayerOutput<T, N>& output, size_t offset = 0) {                             \
        return *ptr_cast<const vec_type>(ptr_cast<const T>(output) + offset);                                          \
    }

DEFINE_VEC_AT(ivec_t, ivec_at)
DEFINE_VEC_AT(fvec_t, fvec_at)

#undef DEFINE_VEC_AT

template <typename T>
void transpose(T* weights, int rows, int cols) {
    std::vector<T> transposed(cols * rows);
    for (int i = 0; i < cols; i++)
        for (int j = 0; j < rows; j++)
            transposed[i * rows + j] = weights[j * cols + i];
    std::memcpy(weights, transposed.data(), sizeof(T) * cols * rows);
}

} // namespace nnue
