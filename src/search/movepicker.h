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
        GOOD_NOISY,
        KILLER1,
        KILLER2,
        COUNTER,
        QUIET_AND_BAD_NOISY,

        PC_MOVES,

        Q_MOVES,
        Q_IN_CHECK_MOVES
    };

    class MovePicker
    {
    public:
        int see_cutoff = 0;

        MovePicker(SearchType st, Board &board, History &history, Stack *ss, Move &tt_move, bool in_check = false, bool gen_checks = true);

        Move nextMove();

        int getMoveCount() { return ml_size; }
        
    private:
        int idx = 0;
        Stage stage = TT;

        SearchType st;
        Board &board;
        const History &history;
        const Stack *ss;
        MoveList ml;

        Move ml_tt_move = NO_MOVE;
        Move tt_move = NO_MOVE;
        Move killer1 = NO_MOVE;
        Move killer2 = NO_MOVE;
        Move counter = NO_MOVE;

        bool in_check;
        bool use_checker;
        int ml_size;

        int getQuietScore(Move& m);
        void scoreMoves();
        void partialInsertionSort(int current_idx);
    };

} // namespace Astra

#endif // MOVEPICKER_H
