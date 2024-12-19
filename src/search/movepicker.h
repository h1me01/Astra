#ifndef MOVEPICKER_H
#define MOVEPICKER_H

#include "search.h"
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
        PLAY_KILLER1,
        PLAY_KILLER2,
        PLAY_COUNTER,
        GEN_QUIETS,
        PLAY_QUIETS,
        PLAY_BAD_NOISY,

        GEN_QUIET_CHECKERS,
        PLAY_QUIET_CHECKERS,
    };

    class MovePicker
    {
    public:
        bool skip_quiets = false;
        int see_cutoff = 0;

        MovePicker(SearchType st, const Board &board, const History &history, const Stack *ss, Move &tt_move, bool gen_checks = false);

        Move nextMove();

    private:
        int idx;
        Stage stage;

        SearchType st;
        const Board &board;
        const History &history;
        const Stack *ss;

        bool gen_checkers;

        MoveList ml_main;
        MoveList ml_bad_noisy;
        Move tt_move, killer1, killer2, counter;

        void scoreQuietMoves();
        void scoreNoisyMoves();
    };

} // namespace Astra

#endif // MOVEPICKER_H
