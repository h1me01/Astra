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

enum NodeType : uint8_t {
    ROOT,
    PV,
    NON_PV,
};

struct Limits {
    Time time;
    U64 nodes = 0;
    int depth = MAX_PLY - 1;
    int multipv = 1;
    bool infinite = false;
    bool minimal = false;
    std::vector<std::string> search_moves{};
};

struct RootMove : public Move {
    RootMove() = default;

    explicit RootMove(Move m)
        : Move(m) {}

    RootMove(const RootMove& other) = default;
    RootMove& operator=(const RootMove& other) = default;

    RootMove(RootMove&& other) noexcept
        : Move(std::move(other)),
          sel_depth(other.sel_depth),
          nodes(other.nodes),
          score(other.score),
          avg_score(other.avg_score),
          pv(std::move(other.pv)) {}

    RootMove& operator=(RootMove&& other) noexcept {
        if (this != &other) {
            Move::operator=(std::move(other));
            sel_depth = other.sel_depth;
            nodes = other.nodes;
            score = other.score;
            avg_score = other.avg_score;
            pv = std::move(other.pv);
        }
        return *this;
    }

    int sel_depth = 0;
    U64 nodes = 0;
    Score score = SCORE_NONE;
    Score avg_score = SCORE_NONE;

    PVLine pv{};
};

class Search {
  public:
    Search()
        : exiting(false),
          searching(false) {
        clear_histories();
    }

    bool exiting;
    bool searching;

    std::mutex mutex;
    std::condition_variable cv;

    Board board;
    Limits limits;

    void idle();
    void clear_histories();

    U64 get_nodes() const {
        return nodes.load(std::memory_order_relaxed);
    }

    U64 get_tb_hits() const {
        return tb_hits.load(std::memory_order_relaxed);
    }

    int get_completed_depth() const {
        return completed_depth;
    }

    const RootMove& get_best_move() const {
        return root_moves[0];
    }

  private:
    TimeMan tm;

    nnue::AccumList accum_list;
    MoveList<RootMove> root_moves;

    QuietHistory quiet_history;
    NoisyHistory noisy_history;
    PawnHistory pawn_history;
    ContinuationHistory cont_history;
    CorrectionHistories corr_histories;
    ContinuationCorrectionHistory cont_corr_history;

    std::atomic<uint64_t> nodes, tb_hits;

    int multipv_idx;
    int sel_depth;
    int root_depth;
    int completed_depth;
    int nmp_min_ply;

    void start();

    Score aspiration(int depth, Stack* stack);

    template <NodeType nt>
    Score negamax(int depth, Score alpha, Score beta, Stack* stack, bool cut_node = false);

    template <NodeType nt>
    Score quiescence(Score alpha, Score beta, Stack* stack);

    void make_move(Move move, Stack* stack);
    void undo_move(Move move);

    int reduction(int depth, int moves_made) const;

    Score evaluate();
    Score adjust_eval(int32_t eval, Stack* stack) const;

    Score draw_score() const;

    unsigned int probe_wdl() const;

    bool is_limit_reached() const;

    void sort_root_moves(int offset);
    bool found_root_move(Move move);

    void update_pv(Move move, Stack* stack);

    void update_quiet_histories(Move best_move, int bonus, Stack* stack);
    void update_histories(Move best_move, MoveList<Move>& quiets, MoveList<Move>& noisy, int depth, Stack* stack);

    void print_uci_info() const;
};

} // namespace search
