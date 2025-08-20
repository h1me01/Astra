#pragma once

#include "../chess/board.h"

#include "stack.h"
#include "timeman.h"
#include "tt.h"
#include "history.h"

using namespace Chess;

namespace Search {

struct PVLine {
    Move pv[MAX_PLY + 1];
    uint8_t length = 0;

    Move &operator[](int depth) {
        return pv[depth];
    }

    Move operator[](int depth) const {
        return pv[depth];
    }
};

class Search {
  public:
    Search(Board &board) : total_nodes{0}, board{board} {}

    void start(Limits limits);

    Move get_best_move() const {
        return pv_table[0][0];
    }

  private:
    int root_depth;
    U64 total_nodes;

    Limits limits;
    TimeMan tm;
    History history;

    Board &board;
    PVLine pv_table[MAX_PLY + 1];

    void update_pv(const Move &move, int ply);

    bool is_limit_reached(int depth) const;

    void print_uci_info(Score score) const;

    Score negamax(int depth, Score alpha, Score beta, Stack* s);
    Score quiescence(Score alpha, Score beta, Stack* s);
};

} // namespace Search
