#ifndef MOVEPICKER_H
#define MOVEPICKER_H

#include "search.h"
#include "../chess/movegen.h"

namespace Astra
{
    enum Stage : int
    {
        TT,
        EVAL,
        GOOD_CAPTURES,
        KILLERS_1,
        KILLERS_2,
        COUNTER,
        BAD
    };

    enum MoveScores : int
    {
        CAPTURE_SCORE = 7'000'000,
        KILLER1_SCORE = 6'000'000,
        KILLER2_SCORE = 5'000'000,
        COUNTER_SCORE = 4'000'000
    };

    // static exchange evaluation from Weiss
    bool see(const Board &board, Move move, int threshold);

    class MovePicker
    {
    public:
        MovePicker(MoveType mt, Board& board, const History& history, const Stack *ss, Move tt_move);
        
        Move nextMove();

        int getMoveCount() { return ml.size(); }

    private:
        int stage = TT;

        MoveType mt;
        Board& board;
        const History& history;
        const Stack *ss;
        MoveList ml;
        
        Move tt_move = NO_MOVE;
        Move killer1 = NO_MOVE;
        Move killer2 = NO_MOVE;
        Move counter = NO_MOVE;
        
        int idx = 0;

        void evaluateMoves();
        void swapMoves(int i, int j);
       
        int partialInsertionSort(int start);
    };

} // namespace Astra

#endif // MOVEPICKER_H
