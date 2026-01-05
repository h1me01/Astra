#pragma once

#include "../chess/movegen.h"
#include "search.h"
#include "stack.h"

using namespace chess;

namespace search {

enum SearchType {
    N_SEARCH,
    Q_SEARCH,
    PC_SEARCH,
};

enum Stage {
    PLAY_TT_MOVE,
    GEN_NOISY,
    PLAY_NOISY,
    GEN_QUIETS,
    PLAY_QUIETS,
    PLAY_BAD_NOISY,
};

template <SearchType st> //
class MovePicker {
  public:
    MovePicker(                      //
        Board &board,                //
        Move tt_move,                //
        QuietHistory &quiet_history, //
        PawnHistory &pawn_history,   //
        NoisyHistory &noisy_history, //
        Stack *stack);

    int probcut_threshold = 0;

    Move next(bool skip_quiets = false);

  private:
    int idx;
    Stage stage;

    const Board &board;
    const QuietHistory &quiet_history;
    const PawnHistory &pawn_history;
    const NoisyHistory &noisy_history;
    const Stack *stack;

    Move tt_move;
    MoveList<ScoredMove> ml_main, ml_bad_noisy;

    void gen_score_noisy();
    void gen_score_quiets();
};

} // namespace search
