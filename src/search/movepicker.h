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
    PLAY_KILLER,
    PLAY_COUNTER,
    GEN_QUIETS,
    PLAY_QUIETS,
    PLAY_BAD_NOISY,
};

class MovePicker {
  public:
    MovePicker(SearchType st, const Board &board, const History &history, const Stack *stack, const Move &tt_move);

    int probcut_threshold = 0;

    Move next(bool skip_quiets = false);

    Move get_killer() const {
        return killer;
    }

    Move get_counter() const {
        return counter;
    }

  private:
    int idx;
    Stage stage;

    SearchType st;
    const Board &board;
    const History &history;
    const Stack *stack;

    Move tt_move, killer, counter;
    MoveList<ScoredMove> ml_main, ml_bad_noisy;

    void gen_score_noisy();
    void gen_score_quiets();
};

} // namespace search
