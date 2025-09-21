#pragma once

#include <condition_variable>
#include <mutex>

#include "../chess/board.h"
#include "../chess/movegen.h"

#include "history.h"
#include "stack.h"
#include "timeman.h"
#include "tt.h"

using namespace Chess;

namespace Engine {

void init_reductions();

enum class NodeType { //
    ROOT,
    PV,
    NON_PV
};

struct RootMove : Move {
    RootMove() = default;
    RootMove(Move m) : Move(m) {}

    // public variables

    U64 nodes = 0;
    Score avg_score = VALUE_NONE;
    PVLine pv{};
};

class Search {
  public:
    Search() {
        history.clear();
    }

    // public variables

    bool searching = false;
    bool exiting = false;

    int completed_depth = 0;

    std::mutex mutex;
    std::condition_variable cv;

    Limits limits;
    Board board{STARTING_FEN};

    MoveList<RootMove> root_moves;

    // public functions

    void idle();

    void print_uci_info() const;

    void clear_histories() {
        history.clear();
    }

    U64 get_total_nodes() const {
        return total_nodes;
    }

    U64 get_tb_hits() const {
        return tb_hits;
    }

  private:
    // private variables

    int root_depth;
    int multipv_idx;

    U64 total_nodes;
    U64 tb_hits;

    int ply;
    int nmp_min_ply;

    TimeMan tm;
    History history;

    NNUE::AccumList accum_list;

    // private functions

    void start();

    Score aspiration(int depth, Stack *stack);

    template <NodeType nt> //
    Score negamax(int depth, Score alpha, Score beta, Stack *stack, bool cut_node = false);

    template <NodeType nt> //
    Score quiescence(int depth, Score alpha, Score beta, Stack *stack);

    void make_move(const Move &move, Stack *stack);
    void undo_move(const Move &move);

    Score evaluate();
    Score adjust_eval(int32_t eval, Stack *stack) const;

    unsigned int probe_wdl() const;

    bool is_limit_reached(int depth) const;

    void sort_rootmoves(int offset);
    bool found_rootmove(const Move &move);

    void update_pv(const Move &move, Stack *stack);
};

} // namespace Engine
