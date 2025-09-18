#include "cuckoo.h"
#include "bitboard.h"
#include "zobrist.h"

namespace Chess::Cuckoo {

U64 keys[8192];
Move cuckoo_moves[8192];

void init() {
    for(int i = 0; i < 8192; i++) {
        keys[i] = 0;
        cuckoo_moves[i] = NO_MOVE;
    }

    int count = 0;
    for(Color c : {WHITE, BLACK}) {
        for(PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
            Piece p = make_piece(c, pt);

            for(Square sq1 = a1; sq1 <= h8; ++sq1) {
                for(Square sq2 = Square(sq1 + 1); sq2 <= h8; ++sq2) {
                    if(!(get_attacks(pt, sq1, 0) & sq_bb(sq2)))
                        continue;

                    Move move = Move(sq1, sq2, QUIET);
                    U64 hash = Zobrist::get_psq(p, sq1) ^ Zobrist::get_psq(p, sq2) ^ Zobrist::get_side();

                    int i = cuckoo_h1(hash);
                    while(true) {
                        std::swap(keys[i], hash);
                        std::swap(cuckoo_moves[i], move);

                        if(!move)
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

} // namespace Chess::Cuckoo
