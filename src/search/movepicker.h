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
        PLAY_KILLER1,
        PLAY_KILLER2,
        PLAY_COUNTER,
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

        MovePicker(SearchType st, const Board &board, const History &history, const Stack *ss, Move &tt_move, bool gen_checks = false);

        Move nextMove();
        
    private:
        int idx;
        Stage stage;

        SearchType st;
        const Board &board;
        const History &history;
        const Stack *ss;

        MoveList ml_main;
        MoveList ml_bad_noisy;
        Move tt_move, killer1, killer2, counter;

        bool gen_checkers;

        void scoreQuietMoves();
        void scoreNoisyMoves();
        void partialInsertionSort(MoveList& ml, int current_idx);
    };

} // namespace Astra

#endif // MOVEPICKER_H
