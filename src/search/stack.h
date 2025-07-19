#pragma once

#include "../chess/types.h"

using namespace Chess;

namespace Astra {

using ContH = int16_t[NUM_PIECES + 1][NUM_SQUARES + 1];

struct Stack {
    int ply;
    int move_count;

    bool was_cap;

    Score static_eval;
    Piece moved_piece;

    Move killer;
    Move skipped;
    Move move;

    ContH *conth;
};

} // namespace Astra
