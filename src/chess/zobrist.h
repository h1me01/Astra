#pragma once

#include "types.h"

namespace astra::zobrist {

void init();

Hash side();
Hash ep_sq(Square sq);
Hash castling(int idx);
Hash psq(Piece pc, Square sq);

} // namespace astra::zobrist
