#pragma once

#include "../chess/types.h"

using namespace Chess;

namespace Astra {

using ContH = int16_t[NUM_PIECES][NUM_SQUARES];

struct Stack {
    int ply;
    int move_count = 0;

    bool was_cap = false;

    Score static_eval = VALUE_NONE;
    Piece moved_piece = NO_PIECE;

    Move killer = NO_MOVE;
    Move skipped = NO_MOVE;
    Move curr_move = NO_MOVE;

    ContH *conth;
};

} // namespace Astra
