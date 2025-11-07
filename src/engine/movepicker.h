#pragma once

#include "../chess/movegen.h"
#include "search.h"
#include "stack.h"

using namespace Chess;

namespace Engine {

enum SearchType : int { //
    N_SEARCH,
    Q_SEARCH,
    PC_SEARCH
};

enum Stage : int {
    PLAY_TT_MOVE,
    GEN_NOISY,
    PLAY_NOISY,
    PLAY_KILLER,
    PLAY_COUNTER,
    GEN_QUIETS,
    PLAY_QUIETS,
    PLAY_BAD_NOISY,

    GEN_QUIET_CHECKERS,
    PLAY_QUIET_CHECKERS,
};

class MovePicker {
  public:
    MovePicker(SearchType st,          //
               const Board &board,     //
               const History &history, //
               const Stack *stack,     //
               const Move &tt_move,    //
               bool gen_checks = false //
    );

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

    bool gen_checks;
    Move tt_move, killer, counter;
    MoveList<> ml_main, ml_bad_noisy;

    void gen_score_noisy();
    void gen_score_quiets();
};

} // namespace Engine
