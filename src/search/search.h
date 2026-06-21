#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "../chess/board.h"
#include "../chess/movegen.h"

#include "history.h"
#include "timeman.h"
#include "tt.h"
#include "types.h"

namespace astra::search {

enum class Node : uint8_t { ROOT, PV, NON_PV };

struct RootMove : public Move {
    RootMove() = default;
    explicit RootMove(Move m)
        : Move(m) {}

    int sel_depth = 0;
    uint64_t nodes = 0;
    Score score = SCORE_NONE;
    Score avg_score = SCORE_NONE;
    PVLine pv{};
};

class Search {
  public:
    Search() { clear_histories(); }

    bool exiting = false;
    bool searching = false;

    std::mutex mutex;
    std::condition_variable cv;

    Board board;
    Limits limits;

    void idle();
    void clear_histories();

    uint64_t nodes() const { return nodes_.load(std::memory_order_relaxed); }
    uint64_t tb_hits() const { return tb_hits_.load(std::memory_order_relaxed); }
    int completed_depth() const { return completed_depth_; }
    const RootMove& best_root_move() const { return root_moves_[0]; }

  private:
    TimeMan tm_;

    nnue::AccumulatorList accum_list_;
    MoveList<RootMove> root_moves_;

    QuietHistory quiet_history_;
    NoisyHistory noisy_history_;
    PawnHistory pawn_history_;
    ContinuationHistory cont_history_;
    CorrectionHistories corr_histories_;
    ContinuationCorrectionHistory cont_corr_history_;

    std::atomic<uint64_t> nodes_, tb_hits_;

    int multipv_idx_;
    int sel_depth_;
    int root_depth_;
    int completed_depth_;
    int nmp_min_ply_;

    void start();

    Score aspiration(int depth, Stack* stack);

    template <Node node>
    Score negamax(int depth, Score alpha, Score beta, Stack* stack, bool cut_node = false);

    template <Node node>
    Score quiescence(Score alpha, Score beta, Stack* stack);

    void make_move(Move move, Stack* stack);
    void undo_move(Move move);

    Score evaluate();
    Score adjust_eval(int32_t eval, int correction_val) const;
    Score draw_score() const;
    Score normalize_score(Score score) const;

    int correction_value(Stack* stack) const;

    unsigned int probe_wdl() const;
    bool limit_reached() const;
    void sort_root_moves(int offset);
    bool found_root_move(Move move);
    void update_quiet_histories(Move best_move, int bonus, Stack* stack);
    void update_histories(Move best_move, MoveList<Move>& quiets, MoveList<Move>& noisy, int depth, Stack* stack);
    void print_uci_info() const;
};

} // namespace astra::search
