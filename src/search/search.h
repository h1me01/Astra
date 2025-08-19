#pragma once

#include "../chess/board.h"

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
    Search() : ply{0}, nodes_count{0}, board{STARTING_FEN} {}

    void start();

    Move best_move() const {
        return pv_table[0][0];
    }

  private:
    int root_depth;
    int ply;
    U64 nodes_count;

    Board board;
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

    Score negamax(int depth, Score alpha, Score beta);
};

} // namespace Search
