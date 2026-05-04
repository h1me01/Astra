#pragma once

#include <cstring>
#include <vector>

#include "simd.h"

namespace astra::nnue {

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

template <typename To, typename From>
const To* ptr_cast(const From* ptr) {
    return reinterpret_cast<const To*>(ptr);
}

template <typename T>
simd::fvec_t& fvec_at(T* ptr, size_t offset = 0) {
    return *ptr_cast<simd::fvec_t>(ptr + offset);
}

template <typename T>
const simd::fvec_t& fvec_at(const T* ptr, size_t offset = 0) {
    return *ptr_cast<const simd::fvec_t>(ptr + offset);
}

template <typename T>
simd::ivec_t& ivec_at(T* ptr, size_t offset = 0) {
    return *ptr_cast<simd::ivec_t>(ptr + offset);
}

template <typename T>
const simd::ivec_t& ivec_at(const T* ptr, size_t offset = 0) {
    return *ptr_cast<const simd::ivec_t>(ptr + offset);
}

#undef DEFINE_VEC_AT

} // namespace astra::nnue
