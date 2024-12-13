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
        TT,
        GEN_NOISY,
        PLAY_GOOD_NOISY,
        KILLER1,
        KILLER2,
        COUNTER,
        GEN_QUIETS,
        PLAY_QUIETS,
        PLAY_BAD_NOISY,

        PC_GEN_NOISY,
        PC_PLAY_GOOD_NOISY,

        Q_GEN_NOISY,
        Q_PLAY_NOISY,
        Q_GEN_QUIET_CHECKERS,
        Q_PLAY_QUIET_CHECKERS,

        Q_IN_CHECK_TT,
        Q_IN_CHECK_GEN_NOISY,
        Q_IN_CHECK_PLAY_NOISY,
        Q_IN_CHECK_GEN_QUIETS,
        Q_IN_CHECK_PLAY_QUIETS,
    };

    class MovePicker
    {
    public:
        bool skip_quiets = false;
        int see_cutoff = 0;

        MovePicker(SearchType st, const Board &board, const History &history, const Stack *ss, Move &tt_move, bool in_check = false, bool gen_checks = false);

        Move nextMove();
        
    private:
        int idx = 0;
        Stage stage = TT;

        SearchType st;
        const Board &board;
        const History &history;
        const Stack *ss;
        MoveList ml_main;
        MoveList ml_bad_noisy;

        Move tt_move = NO_MOVE;
        Move killer1 = NO_MOVE;
        Move killer2 = NO_MOVE;
        Move counter = NO_MOVE;

        bool in_check;
        bool gen_checkers;

        void scoreQuietMoves();
        void scoreNoisyMoves();
        void partialInsertionSort(MoveList& ml, int current_idx);
    };

} // namespace Astra

#endif // MOVEPICKER_H
