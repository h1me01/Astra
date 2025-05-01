#pragma once

#include "search.h"
#include "stack.h"
#include "../chess/movegen.h"

namespace Astra
{
    enum SearchType : int
    {
        N_SEARCH,
        Q_SEARCH,
        PC_SEARCH
    };

    enum Stage : int
    {
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

    class MovePicker
    {
    private:
        int idx;
        Stage stage;

        SearchType st;
        const Board &board;
        const History &history;
        const Stack *ss;

        bool gen_checkers;
        bool skip_quiets = false;

        MoveList<> ml_main;
        MoveList<> ml_bad_noisy;

        void scoreQuietMoves();
        void scoreNoisyMoves();

    public:
        int see_cutoff = 0;

        MovePicker(SearchType st, const Board &board, const History &history, const Stack *ss, const Move &tt_move, bool gen_checks = false);

        Move nextMove();
        void skipQuiets() { skip_quiets = true; }

        Move tt_move, killer, counter;
    };

} // namespace Astra
