#pragma once

#include "../chess/board.h"
#include "../chess/movegen.h"

#include "history.h"
#include "stack.h"
#include "timeman.h"
#include "tt.h"

using namespace Chess;

namespace Search {

void init_reductions();

enum class NodeType { //
    ROOT,
    PV,
    NON_PV
};

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

struct RootMove {
    Move move = NO_MOVE;
    Score score = -VALUE_INFINITE;
    Score avg_score = VALUE_NONE;
    U64 nodes = 0;
    PVLine pv;
};

class Search {
  public:
    Search() : total_nodes{0}, tb_hits{0} {}

    void start(const Board &board, Limits limits);

    U64 get_total_nodes() const {
        return total_nodes;
    }

    U64 get_tb_hits() const {
        return tb_hits;
    }

    int id = 0;
    bool use_tb = false;

  private:
    int root_depth;
    int multipv_idx;

    U64 total_nodes;
    U64 tb_hits;

    Limits limits;
    TimeMan tm;
    History history;

    Board board{STARTING_FEN, false};
    PVLine pv_table[MAX_PLY + 1];

    MoveList<RootMove> root_moves;

    Score aspiration(int depth, Stack *s);

    template <NodeType nt> //
    Score negamax(int depth, Score alpha, Score beta, Stack *s, bool cut_node);

    template <NodeType nt> //
    Score quiescence(int depth, Score alpha, Score beta, Stack *s);

    Score evaluate();
    Score adjust_eval(Score eval, Stack *s) const;

    unsigned int probe_wdl() const;

    bool is_limit_reached(int depth) const;

    void sort_rootmoves(int offset);
    bool found_rootmove(const Move &move);

    void update_pv(const Move &move, int ply);

    void print_uci_info() const;
};

} // namespace Search
