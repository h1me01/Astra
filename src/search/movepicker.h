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
        REST
    };

    class MovePicker
    {
    public:
        MovePicker(MoveType mt, Board& board, const History& history, const Stack *ss, Move& tt_move);
        
        Move nextMove();

        int getMoveCount() { return ml.size(); }

        Move killer1 = NO_MOVE;
        Move killer2 = NO_MOVE;
        Move counter = NO_MOVE;

    private:
        int stage = TT;

        MoveType mt;
        Board& board;
        const History& history;
        const Stack *ss;
        MoveList ml;
        
        Move tt_move = NO_MOVE;
        
        int idx = 0;
        int ml_size;

        void evaluateMoves();       
        void partialInsertionSort(int start);
    };

} // namespace Astra

#endif // MOVEPICKER_H
