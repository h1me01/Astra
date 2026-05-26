#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>

namespace astra {

template <typename T, size_t... Dims>
class NDArray {
  public:
    static constexpr size_t ndim = sizeof...(Dims);
    static constexpr size_t total = (Dims * ...);
    static constexpr std::array<size_t, ndim> shape = {Dims...};
    static constexpr std::array<size_t, ndim> strides = []() {
        std::array<size_t, ndim> s{};
        size_t stride = 1;
        for (int i = static_cast<int>(ndim) - 1; i >= 0; --i) {
            s[i] = stride;
            stride *= shape[i];
        }
        return s;
    }();

    constexpr NDArray()
        : data_{} {}

    constexpr explicit NDArray(T value)
        : data_{} {
        fill(value);
    }

    constexpr NDArray(std::initializer_list<T> il)
        : data_{} {
        assert(il.size() == total);
        std::copy(il.begin(), il.end(), data_);
    }

    NDArray& operator=(std::initializer_list<T> il) {
        assert(il.size() == total);
        std::copy(il.begin(), il.end(), data_);
        return *this;
    }

    constexpr void fill(T value) { std::fill(begin(), end(), value); }

    template <typename... Idx>
    constexpr T& operator()(Idx... indices) noexcept {
        static_assert(sizeof...(Idx) == ndim, "NDArray: wrong number of indices");
        check_bounds<0>(indices...);
        return data_[flat_offset<0>(indices...)];
    }

    template <typename... Idx>
    constexpr const T& operator()(Idx... indices) const noexcept {
        static_assert(sizeof...(Idx) == ndim, "NDArray: wrong number of indices");
        check_bounds<0>(indices...);
        return data_[flat_offset<0>(indices...)];
    }

    constexpr T min() const { return *std::ranges::min_element(*this); }
    constexpr T max() const { return *std::ranges::max_element(*this); }

    constexpr T* data() noexcept { return data_; }
    constexpr const T* data() const noexcept { return data_; }

    constexpr T* begin() noexcept { return data_; }
    constexpr T* end() noexcept { return data_ + total; }
    constexpr const T* begin() const noexcept { return data_; }
    constexpr const T* end() const noexcept { return data_ + total; }

  private:
    T data_[total];

    template <size_t Axis, typename First, typename... Rest>
    static constexpr size_t flat_offset(First i, Rest... rest) noexcept {
        if constexpr (sizeof...(Rest) == 0)
            return static_cast<size_t>(i) * strides[Axis];
        else
            return static_cast<size_t>(i) * strides[Axis] + flat_offset<Axis + 1>(rest...);
    }

    template <size_t Axis, typename First, typename... Rest>
    static void check_bounds([[maybe_unused]] First i, Rest... rest) {
        assert(static_cast<size_t>(i) < shape[Axis]);
        if constexpr (sizeof...(Rest) > 0)
            check_bounds<Axis + 1>(rest...);
    }
};

} // namespace astra
