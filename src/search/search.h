#pragma once

#include <algorithm>

#include "../chess/movegen.h"
#include "history.h"
#include "timeman.h"
#include "tt.h"

using namespace Chess;

namespace Astra {

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

void init_reductions();

enum NodeType {
    ROOT,
    PV,
    NON_PV,
};

class Search {
  public:
    int id = 0; // main thread
    bool use_tb = false;

    Board board;
    Limits limit;

    Search(const std::string &fen);

    Move bestmove();

    U64 get_nodes() const {
        return nodes;
    }

    U64 get_tb_hits() const {
        return tb_hits;
    }

  private:
    int multipv_idx;
    int root_depth;
    U64 nodes;
    U64 tb_hits;

    TimeMan tm;
    History history;
    PVLine pv_table[MAX_PLY + 1];

    MoveList<RootMove> rootmoves;

    Score aspiration(int depth, Stack* ss);

    template <NodeType nt> Score qsearch(int depth, Score alpha, Score beta, Stack *ss);
    template <NodeType nt> Score negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node);

    Score evaluate();
    Score adjust_eval(const Stack *ss, Score eval) const;

    bool is_limit_reached(int depth) const;
    void update_pv(Move move, int ply);

    void sort_rootmoves(int offset);
    bool found_rootmove(const Move &move);

    void print_uci_info();
};

} // namespace Astra
