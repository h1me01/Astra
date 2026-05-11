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

template <GenType gt>
Move* gen_moves(const Board& board, Move* ml);

template <typename T>
class MoveList {
  public:
    static constexpr int MAX_MOVES = 128;

    MoveList() { clear(); }

    void clear() { last_ = list_; }

    template <GenType gt>
    void gen(const Board& board) {
        clear();

        if constexpr (std::is_same_v<T, Move>) {
            last_ = gen_moves<gt>(board, list_);
        } else {
            Move temp_list[MAX_MOVES];
            Move* temp_last = gen_moves<gt>(board, temp_list);

            for (Move* it = temp_list; it != temp_last; ++it)
                *last_++ = T(*it);
        }
    }

    void add(T m) {
        assert(last_ < list_ + MAX_MOVES);
        *last_++ = m;
    }

    int idx_of(const T move) const {
        auto it = std::ranges::find(begin(), end(), move);
        return it != end() ? static_cast<int>(it - begin()) : -1;
    }

    T* begin() { return list_; }
    const T* begin() const { return list_; }
    T* end() { return last_; }
    const T* end() const { return last_; }

    T& operator[](int i) {
        assert(i >= 0 && i < size());
        return list_[i];
    }

    const T& operator[](int i) const {
        assert(i >= 0 && i < size());
        return list_[i];
    }

    int size() const { return last_ - list_; }
    bool empty() const { return size() == 0; }

  private:
    T list_[MAX_MOVES];
    T* last_;
};

} // namespace astra
