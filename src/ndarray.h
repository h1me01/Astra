#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <ostream>
#include <string>

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

    friend std::ostream& operator<<(std::ostream& os, const NDArray& arr) {
        os << "NDArray<";
        for (size_t i = 0; i < ndim; ++i) {
            os << shape[i];
            if (i + 1 < ndim)
                os << "x";
        }
        os << ">\n";
        arr.print_recursive(os, 0, 0, 0);
        return os;
    }

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

    void print_recursive(std::ostream& os, size_t dim, size_t offset, int depth) const {
        std::string indent(depth * 2, ' ');
        if (dim == ndim - 1) {
            os << indent << "[";
            for (size_t i = 0; i < shape[dim]; ++i) {
                os << data_[offset + i];
                if (i + 1 < shape[dim])
                    os << ", ";
            }
            os << "]\n";
        } else {
            os << indent << "[\n";
            for (size_t i = 0; i < shape[dim]; ++i)
                print_recursive(os, dim + 1, offset + i * strides[dim], depth + 1);
            os << indent << "]\n";
        }
    }
};

} // namespace astra
