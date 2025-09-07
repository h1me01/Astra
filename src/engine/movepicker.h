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

    // public variables

    int see_cutoff = 0;

    // public functions

    Move next();

    void skip_quiets() {
        m_skip_quiets = true;
    }

    Move get_killer() const {
        return killer;
    }

    Move get_counter() const {
        return counter;
    }

  private:
    // private variables

    int idx;
    Stage stage;

    SearchType st;
    const Board &board;
    const History &history;
    const Stack *stack;

    bool gen_checks;
    bool m_skip_quiets = false;

    Move tt_move, killer, counter;

    MoveList<> ml_main;
    MoveList<> ml_bad_noisy;

    // private functions

    void score_quiets();
    void score_noisy();
};

} // namespace Engine
