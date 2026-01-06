#pragma once

#include "types.h"

namespace chess {

class Board;

namespace zobrist {

void init();

U64 side_hash();
U64 ep_hash(Square sq);
U64 castling_hash(int idx);
U64 psq_hash(Piece pc, Square sq);

} // namespace zobrist

} // namespace chess
