#ifndef MOVEPICKER_H
#define MOVEPICKER_H

#include "search.h"
#include "../chess/movegen.h"

namespace Astra
{
    enum Stage : int
    {
        TT,
        CAPTURES,
        KILLERS_1,
        KILLERS_2,
        COUNTER,
        QUIET
    };

    enum MoveScores : int
    {
        TT_SCORE = 10'000'000,
        CAPTURE_SCORE = 7'000'000,
        KILLER_ONE_SCORE = 6'000'000,
        KILLER_TWO_SCORE = 5'000'000,
        COUNTER_SCORE = 4'000'000
    };

    // static exchange evaluation from Weiss
    bool see(const Board &board, Move move, int threshold);

    class MovePicker
    {
    public:
        MovePicker(MoveType mt, const Search& search, const Stack *ss, Move tt_move);
        
        Move nextMove();

        int getMoveCount() { return ml.size(); }

    private:
        int stage = TT;

        MoveType mt;
        const Search& search;
        const Stack *ss;
        MoveList ml;
        
        Move tt_move = NO_MOVE;
        Move killer1 = NO_MOVE;
        Move killer2 = NO_MOVE;
        Move counter_move = NO_MOVE;
        
        int idx;

        void scoreMoves();
        void partialInsertionSort(const int move_num);

        int getHistoryScore(Move m) const;
        Move getCounterMove(Move m) const;
    };

} // namespace Astra

#endif // MOVEPICKER_H
