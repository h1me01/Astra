#pragma once

#include "castling_rights.h"
#include "types.h"

namespace Chess {

struct StateInfo {
    StateInfo() = default;
    StateInfo(const StateInfo &other) = default;
    StateInfo &operator=(const StateInfo &other) = default;

    Piece captured;
    Square ep_sq;
    CastlingRights castle_rights;

    int fmr_counter; // fifty move rule
    int plies_from_null;

    U64 hash;
    U64 pawn_hash;
    U64 non_pawn_hash[NUM_COLORS];

    U64 occ[NUM_COLORS];
    U64 threats[NUM_PIECE_TYPES];

    U64 checkers;
    U64 danger;

    U64 pinners[NUM_COLORS];
    U64 blockers[NUM_COLORS];
};

} // namespace Chess
