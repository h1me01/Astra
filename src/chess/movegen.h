#pragma once

#include <algorithm>
#include <type_traits>

#include "board.h"

namespace astra {

enum class GenType : uint8_t {
    NOISY,
    QUIET,
    LEGAL,
};

template <typename T>
class MoveList {
  public:
    static constexpr int MAX_MOVES = 256;

    MoveList()
        : idx_(-1) {
        clear();
    }

    void clear() { idx_ = -1; }

    void add(T m) {
        assert(idx_ < MAX_MOVES - 1);
        data_(++idx_) = m;
    }

    int idx_of(const T move) const {
        auto it = std::ranges::find(begin(), end(), move);
        return it != end() ? static_cast<int>(it - begin()) : -1;
    }

    T* begin() { return size() ? &data_(0) : nullptr; }
    T* end() { return size() ? (&data_(0) + size()) : nullptr; }
    const T* begin() const { return size() ? &data_(0) : nullptr; }
    const T* end() const { return size() ? (&data_(0) + size()) : nullptr; }

    T& operator[](int i) {
        assert(i >= 0 && i < size());
        return data_(i);
    }

    const T& operator[](int i) const {
        assert(i >= 0 && i < size());
        return data_(i);
    }

    T& back() {
        assert(!empty());
        return data_(idx_);
    }

    const T& back() const {
        assert(!empty());
        return data_(idx_);
    }

    void pop() {
        assert(!empty());
        --idx_;
    }

    int size() const { return idx_ + 1; }
    bool empty() const { return size() == 0; }

  private:
    int idx_;
    NDArray<T, MAX_MOVES> data_;
};

template <GenType gt>
void gen_moves(MoveList<Move>& ml, const Board& board);

} // namespace astra
