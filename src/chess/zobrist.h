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

U64 side_hash();
U64 ep_hash(Square sq);
U64 castling_hash(int idx);
U64 psq_hash(Piece pc, Square sq);

} // namespace zobrist

} // namespace chess
