#pragma once

#include "board.h"
#include "types.h"

namespace Chess {

class Board;

// psuedorandom number generator from stockfish
class PRNG {
  private:
    U64 s;

    U64 rand64() {
        s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
        return s * 2685821657736338717LL;
    }

  public:
    explicit PRNG(U64 seed) : s(seed) {}

    // generate psuedorandom number
    template <typename T> T rand() {
        return T(rand64());
    }
};

namespace Zobrist {
extern U64 side;

void init();

U64 getPsq(Piece pc, Square sq);
U64 getCastle(int idx);
U64 getEp(Square sq);

U64 getPawnZobrist(const Board &board);
U64 getNonPawnZobrist(const Board &board, Color c);

} // namespace Zobrist

} // namespace Chess
