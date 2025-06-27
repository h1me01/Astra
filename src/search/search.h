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
    Move move;
    Score score;
    Score avg_score;
    int seldepth;
    U64 nodes;
    PVLine pv;
};

void initReductions();

class Search {
  private:
    bool debugging = false;

    int multipv_idx;
    int root_depth;
    U64 nodes = 0;
    U64 tb_hits = 0;
    int seldepth = 0;

    MoveList<RootMove> root_moves;

    PVLine pv_table[MAX_PLY + 1];
    History history;
    TimeMan tm;

    Score aspSearch(int depth, Stack *ss);
    Score negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node, const Move skipped = NO_MOVE);
    Score qSearch(int depth, Score alpha, Score beta, Stack *ss);

    Score evaluate();
    Score adjustEval(const Stack *ss, Score eval) const;

    bool isLimitReached(int depth) const;
    void updatePv(Move move, int ply);

    void sortRootMoves(int offset);
    bool foundRootMove(const Move &move);

    void printUciInfo();

  public:
    int id = 0; // main thread
    bool use_tb = false;

    Board board;
    Limits limit;

    Search(const std::string &fen);

    Move bestMove();

    U64 getNodes() const {
        return nodes;
    }

    U64 getTbHits() const {
        return tb_hits;
    }

    int getSelDepth() const {
        return seldepth;
    }
};

} // namespace Astra
