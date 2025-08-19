#pragma once

#include "../chess/board.h"

#include "timeman.h"

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
    Search(Board &board) : ply{0}, total_nodes{0}, board{board} {}

    void start(Limits limits);

    Move best_move() const {
        return pv_table[0][0];
    }

  private:
    int root_depth;
    int ply;
    U64 total_nodes;

    Limits limits;
    TimeMan tm;

    Board &board;
    PVLine pv_table[MAX_PLY + 1];

    void make_move(const Move &m) {
        board.make_move(m);
        ply++;
    }

    void unmake_move(const Move &m) {
        board.unmake_move(m);
        ply--;
    }

    void update_pv(const Move &move);

    bool is_limit_reached(int depth) const;

    void print_uci_info(Score score) const;

    Score negamax(int depth, Score alpha, Score beta);
};

} // namespace Search
