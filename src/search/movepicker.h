#pragma once

#include "../chess/movegen.h"
#include "search.h"
#include "stack.h"

using namespace Chess;

namespace Search {

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
               const Stack *s,         //
               const Move &tt_move,    //
               bool gen_checks = false //
    );

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

    int see_cutoff = 0;

  private:
    int idx;
    Stage stage;

    SearchType st;
    const Board &board;
    const History &history;
    const Stack *s;

    bool gen_checks;
    bool m_skip_quiets = false;

    Move tt_move, killer, counter;

    MoveList<> ml_main;
    MoveList<> ml_bad_noisy;

    void score_quiets();
    void score_noisy();
};

} // namespace Search
