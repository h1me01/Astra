#pragma once

#include "../chess/misc.h"

using namespace Chess;

namespace Search {

struct Stack {
    int ply;
    
    Move move = NO_MOVE;
    Move killer = NO_MOVE;
};

} // namespace Search
