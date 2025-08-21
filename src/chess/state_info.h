#pragma once

#include "castling_rights.h"
#include "types.h"

namespace Chess {

struct StateInfo {
    StateInfo() = default;
    StateInfo(const StateInfo &other) = default;
    StateInfo &operator=(const StateInfo &other) = default;

    U64 hash;
    Piece captured;
    Square ep_sq;
    CastlingRights castle_rights;
    U64 occ[NUM_COLORS] = {};
    U64 threats[NUM_PIECE_TYPES] = {};
    int fmr_counter; // fifty move rule
    int plies_from_null;

    U64 checkers = 0;
    U64 danger = 0;

    U64 pinners[NUM_COLORS] = {};
    U64 blockers[NUM_COLORS] = {};
};

} // namespace Chess
