#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "../chess/board.h"
#include "../chess/movegen.h"

#include "history.h"
#include "stack.h"
#include "timeman.h"
#include "tt.h"

using namespace chess;

namespace search {

void init_reductions();

enum class NodeType { ROOT, PV, NON_PV };

struct RootMove : Move {
    RootMove() = default;
    RootMove(Move m) : Move(m) {}

    int sel_depth = 0;
    U64 nodes = 0;
    Score avg_score = VALUE_NONE;
    PVLine pv{};
};

class Search {
  public:
    Search() : exiting(false), searching(false) {
        history.clear();
    }

    bool exiting;
    bool searching;

    std::mutex mutex;
    std::condition_variable cv;

    Board board;
    Limits limits;

    void idle();
    void print_uci_info() const;

    void clear_histories() {
        history.clear();
    }

    U64 get_nodes() const {
        return nodes;
    }

    U64 get_tb_hits() const {
        return tb_hits;
    }

    int get_completed_depth() const {
        return completed_depth;
    }

    const RootMove &get_best_rootmove() const {
        return root_moves[0];
    }

  private:
    TimeMan tm;
    History history;

    nnue::AccumList accum_list;
    MoveList<RootMove> root_moves;

    uint64_t nodes, tb_hits;

    int nmp_min_ply;

    int sel_depth;
    int root_depth;
    int completed_depth;

    int multipv_idx;

    void start();

    Score aspiration(int depth, Stack *stack);

    template <NodeType nt> //
    Score negamax(int depth, Score alpha, Score beta, Stack *stack, bool cut_node = false);

    template <NodeType nt> //
    Score quiescence(Score alpha, Score beta, Stack *stack);

    void make_move(const Move &move, Stack *stack);
    void undo_move(const Move &move);

    Score evaluate();
    Score adjust_eval(int32_t eval, Stack *stack) const;

    unsigned int probe_wdl() const;

    bool is_limit_reached() const;

    void sort_rootmoves(int offset);
    bool found_rootmove(const Move &move);

    void update_pv(const Move &move, Stack *stack);
};

} // namespace search
