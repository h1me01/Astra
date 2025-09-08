#include "zobrist.h"
#include "board.h"
#include "misc.h"

namespace Chess::Zobrist {

U64 side;
U64 ep[8];
U64 castle[16];
U64 psq[NUM_PIECES][NUM_SQUARES];

void init() {
    PRNG rng(1070372);

    for(int p = WHITE_PAWN; p <= BLACK_KING; p++)
        for(int sq = a1; sq <= h8; sq++)
            psq[p][sq] = rng.rand<U64>();

    for(int f = FILE_A; f <= FILE_H; f++)
        ep[f] = rng.rand<U64>();

    castle[0] = 0;
    for(int i = 1; i < 16; i++)
        castle[i] = rng.rand<U64>();

    side = rng.rand<U64>();
}

U64 get_side() {
    return side;
}

U64 get_psq(Piece pc, Square sq) {
    assert(valid_sq(sq));
    assert(valid_piece(pc));
    return psq[pc][sq];
}

U64 get_castle(int idx) {
    assert(idx >= 0 && idx < 16);
    return castle[idx];
}

U64 get_ep(Square sq) {
    if(!valid_sq(sq))
        return 0;
    return ep[sq_file(sq)];
}

} // namespace Chess::Zobrist
