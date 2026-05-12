#pragma once

#include "../chess/movegen.h"
#include "search.h"
#include "types.h"

namespace astra::search {

enum SearchType : uint8_t {
    N_SEARCH,
    Q_SEARCH,
    PC_SEARCH,
};

enum Stage : uint8_t {
    PLAY_TT_MOVE,
    GEN_NOISY,
    PLAY_NOISY,
    GEN_QUIETS,
    PLAY_QUIETS,
    PLAY_BAD_NOISY,
};

template <SearchType st>
class MovePicker {
  public:
    MovePicker(
        const Board& board,
        const Move tt_move,
        const QuietHistory& quiet_history,
        const PawnHistory& pawn_history,
        const NoisyHistory& noisy_history,
        const Stack* stack,
        int probcut_threshold = 0
    );

    void skip_quiets() { skip_quiets_ = true; }

    Move next();

  private:
    int idx_;
    Stage stage_;
    bool skip_quiets_ = false;

    const Board& board_;
    const QuietHistory& quiet_history_;
    const PawnHistory& pawn_history_;
    const NoisyHistory& noisy_history_;
    const Stack* stack_;
    int probcut_threshold_;

    Move tt_move_;
    MoveList<Move> ml_;
    MoveList<ScoredMove> ml_main_, ml_bad_noisy_;

    void gen_score_noisy();
    void gen_score_quiets();
};

} // namespace astra::search
