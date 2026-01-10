#include "cuckoo.h"
#include "bitboard.h"
#include "zobrist.h"

namespace chess::cuckoo {

U64 hashes[8192];
Move moves[8192];

U64 hash(const int idx) {
    return hashes[idx];
}

Move move(const int idx) {
    return moves[idx];
}

void init() {
    for (int i = 0; i < 8192; i++) {
        hashes[i] = 0;
        moves[i] = Move::none();
    }

    int count = 0;
    for (Color c : {WHITE, BLACK}) {
        for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            Piece p = make_piece(c, pt);

            for (Square sq1 = a1; sq1 <= h8; ++sq1) {
                for (Square sq2 = Square(sq1 + 1); sq2 <= h8; ++sq2) {
                    if (!(attacks_bb(pt, sq1) & sq_bb(sq2)))
                        continue;

                    Move move = Move(sq1, sq2, QUIET);
                    U64 hash = zobrist::psq_hash(p, sq1) ^ zobrist::psq_hash(p, sq2) ^ zobrist::side_hash();

                    int i = cuckoo_h1(hash);
                    while (true) {
                        std::swap(hashes[i], hash);
                        std::swap(moves[i], move);

                        if (!move)
                            break;

                        i = (i == cuckoo_h1(hash)) ? cuckoo_h2(hash) : cuckoo_h1(hash);
                    }

                    count++;
                }
            }
        }
    }

    assert(count == 3668);
}

} // namespace chess::cuckoo
