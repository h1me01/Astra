#pragma once

#include <cstring>
#include <vector>

#include "simd.h"

namespace astra::nnue {

template <typename T, int... Dims>
struct AlignedArray;

template <typename T, int N>
struct AlignedArray<T, N> {
    alignas(64) T data[N];

    T& operator[](int i) {
        assert(i >= 0);
        assert(i < N);
        return data[i];
    }

    const T& operator[](int i) const {
        assert(i >= 0);
        assert(i < N);
        return data[i];
    }

    operator T*() { return data; }
    operator const T*() const { return data; }
};

template <typename T, int R, int C>
struct AlignedArray<T, R, C> {
    alignas(64) T data[R][C];

    T* operator[](int i) {
        assert(i >= 0);
        assert(i < R);
        return data[i];
    }

    const T* operator[](int i) const {
        assert(i >= 0);
        assert(i < R);
        return data[i];
    }

    operator T*() { return &data[0][0]; }
    operator const T*() const { return &data[0][0]; }
};

template <typename T>
void transpose(T* weights, int rows, int cols) {
    std::vector<T> transposed(cols * rows);
    for (int i = 0; i < cols; i++)
        for (int j = 0; j < rows; j++)
            transposed[i * rows + j] = weights[j * cols + i];
    std::memcpy(weights, transposed.data(), sizeof(T) * cols * rows);
}

template <typename To, typename From>
To* ptr_cast(From* ptr) {
    return reinterpret_cast<To*>(ptr);
}

template <typename To, typename T, int... Dims>
const To* ptr_cast(const AlignedArray<T, Dims...>& output) {
    return reinterpret_cast<const To*>(static_cast<const T*>(output));
}

template <typename To, typename T, int... Dims>
To* ptr_cast(AlignedArray<T, Dims...>& output) {
    return reinterpret_cast<To*>(static_cast<T*>(output));
}

#define DEFINE_VEC_AT(vec_type, func_name)                                                                             \
    template <typename T>                                                                                              \
    vec_type& func_name(T* ptr, size_t offset = 0) {                                                                   \
        return *ptr_cast<vec_type>(ptr + offset);                                                                      \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T>                                                                                              \
    const vec_type& func_name(const T* ptr, size_t offset = 0) {                                                       \
        return *ptr_cast<const vec_type>(ptr + offset);                                                                \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T, int... Dims>                                                                                 \
    vec_type& func_name(AlignedArray<T, Dims...>& output, size_t offset = 0) {                                         \
        return *ptr_cast<vec_type>(ptr_cast<T>(output) + offset);                                                      \
    }                                                                                                                  \
                                                                                                                       \
    template <typename T, int... Dims>                                                                                 \
    const vec_type& func_name(const AlignedArray<T, Dims...>& output, size_t offset = 0) {                             \
        return *ptr_cast<const vec_type>(ptr_cast<const T>(output) + offset);                                          \
    }

DEFINE_VEC_AT(simd::ivec_t, ivec_at)
DEFINE_VEC_AT(simd::fvec_t, fvec_at)

#undef DEFINE_VEC_AT

} // namespace astra::nnue
