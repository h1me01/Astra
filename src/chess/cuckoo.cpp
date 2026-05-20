#include <utility>

#include "../ndarray.h"
#include "bitboard.h"
#include "cuckoo.h"
#include "zobrist.h"

namespace astra::cuckoo {

namespace {
NDArray<Hash, 8192> HASHES;
NDArray<Move, 8192> MOVES;
} // namespace

Hash hash(const int idx) {
    assert(idx >= 0 && idx < 8192);
    return HASHES(idx);
}

Move move(const int idx) {
    assert(idx >= 0 && idx < 8192);
    return MOVES(idx);
}

void init() {
    HASHES.fill(0);
    MOVES.fill(Move::none());

    int count = 0;
    for (Color c : {WHITE, BLACK}) {
        for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            Piece p = make_piece(c, pt);

            for (Square sq1 = SQ_A1; sq1 < NUM_SQUARES; ++sq1) {
                for (Square sq2 = static_cast<Square>(sq1 + 1); sq2 < NUM_SQUARES; ++sq2) {
                    if (!(attacks_bb(pt, sq1) & sq_bb(sq2)))
                        continue;

                    Move move = Move(sq1, sq2, QUIET);
                    Hash hash = zobrist::psq(p, sq1) ^ zobrist::psq(p, sq2) ^ zobrist::side();

                    int i = h1(hash);
                    while (true) {
                        std::swap(HASHES(i), hash);
                        std::swap(MOVES(i), move);

                        if (!move)
                            break;

                        i = (i == h1(hash)) ? h2(hash) : h1(hash);
                    }

                    ++count;
                }
            }
        }
    }

    assert(count == 3668);
}

} // namespace astra::cuckoo
