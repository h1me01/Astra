#pragma once

#include "types.h"

namespace chess {

class Board;

// psuedorandom number generator from stockfish
class PRNG {
  public:
    explicit PRNG(U64 seed) : s(seed) {}

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

namespace zobrist {

void init();

U64 get_side();
U64 get_ep(Square sq);
U64 get_castle(int idx);
U64 get_psq(Piece pc, Square sq);

} // namespace zobrist

} // namespace chess
