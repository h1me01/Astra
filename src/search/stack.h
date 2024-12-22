#ifndef STACK_H
#define STACK_H

#include "../chess/types.h"

using namespace Chess;

namespace Astra 
{
    using ContH = int16_t[NUM_PIECES][NUM_SQUARES];

    struct Stack
    {
        int ply;
        Score eval = VALUE_NONE;

        Piece moved_piece = NO_PIECE;

        Move killer = NO_MOVE;
        Move curr_move = NO_MOVE;

        ContH* conth;
    };
}

#endif // STACK_H