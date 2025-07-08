#pragma once

#include "board.h"
#include "types.h"

namespace Chess {

class Board;

// psuedorandom number generator from stockfish
class PRNG {
  public:
    explicit PRNG(U64 seed) : s(seed) {}

    // generate psuedorandom number
    template <typename T> T rand() {
        return T(rand64());
    }

  private:
    U64 s;

    U64 rand64() {
        s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
        return s * 2685821657736338717LL;
    }
};

namespace Zobrist {

extern U64 side;

void init();

U64 get_psq(Piece pc, Square sq);
U64 get_castle(int idx);
U64 get_ep(Square sq);

U64 get_pawnzobrist(const Board &board);
U64 get_nonpawnzobrist(const Board &board, Color c);

} // namespace Zobrist

} // namespace Chess
