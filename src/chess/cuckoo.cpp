#include "cuckoo.h"
#include "attacks.h"
#include "zobrist.h"

namespace Chess
{
    U64 cuckoo[8192];
    Move cuckoo_moves[8192];

    void initCuckoo()
    {
        int count = 0;

        for (int i = 0; i < 8192; i++)
        {
            cuckoo[i] = 0;
            cuckoo_moves[i] = NO_MOVE;
        }

        for (Color c : {WHITE, BLACK})
            for (PieceType pt : {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING})
            {
                Piece pc = makePiece(c, pt);

                for (Square sq1 = a1; sq1 < h8; ++sq1)
                    for (Square sq2 = Square(sq1 + 1); sq2 < h8; ++sq2)
                    {
                        if (!(getAttacks(pt, sq1, 0) & SQUARE_BB[sq2]))
                            continue;

                        Move move = Move(sq1, sq2, QUIET);

                        U64 hash = Zobrist::getPsq(pc, sq1) ^ Zobrist::getPsq(pc, sq2) ^ Zobrist::side;
                        int i = cuckooH1(hash);

                        while (true)
                        {
                            std::swap(cuckoo[i], hash);
                            std::swap(cuckoo_moves[i], move);

                            if (!move)
                                break;

                            i = (i == cuckooH1(hash)) ? cuckooH2(hash) : cuckooH1(hash);
                        }

                        count++;
                    }
            }

        assert(count != 3668);
    }

} // namespace Chess
