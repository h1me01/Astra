#include "zobrist.h"
#include "../ndarray.h"

namespace astra::zobrist {

namespace {
Hash SIDE;
NDArray<Hash, 8> EP_SQ_HASH;
NDArray<Hash, 16> CASTLING;
NDArray<Hash, NUM_PIECES, NUM_SQUARES> PSQ;
} // namespace

// psuedorandom number generator from stockfish
class PRNG {
  public:
    explicit PRNG(Hash seed)
        : s(seed) {}

    template <typename T>
    T rand() {
        return T(rand64());
    }

  private:
    Hash s;

    Hash rand64() {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717LL;
    }
};

void init() {
    PRNG rng(1070372);

    for (int p = WHITE_PAWN; p <= BLACK_KING; ++p)
        for (int sq = SQ_A1; sq < NUM_SQUARES; ++sq)
            PSQ(p, sq) = rng.rand<Hash>();

    for (int i = FILE_A; i <= FILE_H; ++i)
        EP_SQ_HASH(i) = rng.rand<Hash>();

    CASTLING(0) = 0;
    for (int i = 1; i < 16; ++i)
        CASTLING(i) = rng.rand<Hash>();

    SIDE = rng.rand<Hash>();
}

Hash side() { return SIDE; }

Hash psq(Piece pc, Square sq) {
    assert(is_valid(sq));
    assert(is_valid(pc));
    return PSQ(pc, sq);
}

Hash castling(int idx) {
    assert(idx >= 0 && idx < 16);
    return CASTLING(idx);
}

Hash ep_sq(Square sq) {
    if (!is_valid(sq))
        return 0;
    return EP_SQ_HASH(file_of(sq));
}

} // namespace astra::zobrist
