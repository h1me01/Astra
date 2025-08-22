#pragma once

#include "../chess/misc.h"

using namespace Chess;

namespace Search {

using ContH = int16_t[NUM_PIECES + 1][NUM_SQUARES + 1];

struct Stack {
    int ply;

    Move move = NO_MOVE;
    Move killer = NO_MOVE;
    Move skipped = NO_MOVE;

    Piece moved_piece = NO_PIECE;

    Score static_eval = VALUE_NONE;

    ContH *conth;
};

} // namespace Search
