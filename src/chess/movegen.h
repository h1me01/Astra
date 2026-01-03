#pragma once

#include "board.h"

namespace chess {

enum GenType {
    ADD_NOISY = 1,
    ADD_QUIETS = 2,
    ADD_LEGALS = 3,
};

template <GenType gt> //
Move *gen_moves(const Board &board, Move *ml);

template <typename T> //
class MoveList {
  public:
    MoveList() : last(list) {}

    template <GenType gt> //
    void gen(const Board &board) {
        last = list; // reset the list

        if constexpr(std::is_same_v<T, Move>) {
            last = gen_moves<gt>(board, list);
        } else {
            Move temp_list[MAX_MOVES];
            Move *temp_last = gen_moves<gt>(board, temp_list);

            for(Move *it = temp_list; it != temp_last; ++it)
                *last++ = T(*it);
        }
    }

    void add(T m) {
        assert(last < list + MAX_MOVES);
        *last++ = m;
    }

    int idx_of(const T move) const {
        for(int i = 0; i < size(); i++)
            if(list[i] == move)
                return i;
        return -1;
    }

    T *begin() {
        return list;
    }

    const T *begin() const {
        return list;
    }

    T *end() {
        return last;
    }

    const T *end() const {
        return last;
    }

    T &operator[](int i) {
        return list[i];
    }

    const T &operator[](int i) const {
        return list[i];
    }

    int size() const {
        return last - list;
    }

  private:
    T list[MAX_MOVES];
    T *last;
};

} // namespace chess
