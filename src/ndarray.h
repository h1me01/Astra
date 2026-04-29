#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <ostream>
#include <string>

namespace astra {

template <typename T, std::size_t... Dims>
class NDArray {
  public:
    static constexpr std::size_t ndim = sizeof...(Dims);
    static constexpr std::size_t total = (Dims * ...);
    static constexpr std::array<std::size_t, ndim> shape = {Dims...};
    static constexpr std::array<std::size_t, ndim> strides = []() {
        std::array<std::size_t, ndim> s{};
        std::size_t stride = 1;
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

    constexpr void fill(T value) {
        for (std::size_t i = 0; i < total; ++i)
            data_[i] = value;
    }

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

    constexpr T min() const {
        T min_val = data_[0];
        for (std::size_t i = 1; i < total; ++i)
            if (data_[i] < min_val)
                min_val = data_[i];
        return min_val;
    }

    constexpr T max() const {
        T max_val = data_[0];
        for (std::size_t i = 1; i < total; ++i)
            if (data_[i] > max_val)
                max_val = data_[i];
        return max_val;
    }

    constexpr T* data() noexcept { return data_; }
    constexpr const T* data() const noexcept { return data_; }

    constexpr T* begin() noexcept { return data_; }
    constexpr T* end() noexcept { return data_ + total; }
    constexpr const T* begin() const noexcept { return data_; }
    constexpr const T* end() const noexcept { return data_ + total; }

    friend std::ostream& operator<<(std::ostream& os, const NDArray& arr) {
        os << "NDArray<";
        for (std::size_t i = 0; i < ndim; ++i) {
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

    template <std::size_t Axis, typename First, typename... Rest>
    static constexpr std::size_t flat_offset(First i, Rest... rest) noexcept {
        if constexpr (sizeof...(Rest) == 0)
            return static_cast<std::size_t>(i) * strides[Axis];
        else
            return static_cast<std::size_t>(i) * strides[Axis] + flat_offset<Axis + 1>(rest...);
    }

    template <std::size_t Axis, typename First, typename... Rest>
    static void check_bounds([[maybe_unused]] First i, Rest... rest) {
        assert(static_cast<std::size_t>(i) < shape[Axis]);
        if constexpr (sizeof...(Rest) > 0)
            check_bounds<Axis + 1>(rest...);
    }

    void print_recursive(std::ostream& os, std::size_t dim, std::size_t offset, int depth) const {
        std::string indent(depth * 2, ' ');
        if (dim == ndim - 1) {
            os << indent << "[";
            for (std::size_t i = 0; i < shape[dim]; ++i) {
                os << data_[offset + i];
                if (i + 1 < shape[dim])
                    os << ", ";
            }
            os << "]\n";
        } else {
            os << indent << "[\n";
            for (std::size_t i = 0; i < shape[dim]; ++i)
                print_recursive(os, dim + 1, offset + i * strides[dim], depth + 1);
            os << indent << "]\n";
        }
    }
};

} // namespace astra
