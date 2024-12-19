#include "zobrist.h"
#include "misc.h"

namespace Chess
{
    namespace Zobrist
    {
        U64 psq[NUM_PIECES][NUM_SQUARES];
        U64 castle[16];
        U64 ep[8];
        U64 side;

        void init()
        {
            PRNG rng(1070372);

            for (int pc = WHITE_PAWN; pc <= BLACK_KING; pc++)
                for (int sq = a1; sq <= h8; sq++)
                    psq[pc][sq] = rng.rand<U64>();

            for (int f = FILE_A; f <= FILE_H; f++)
                ep[f] = rng.rand<U64>();

            castle[0] = 0;
            for (int i = 1; i < 16; i++)
                castle[i] = rng.rand<U64>();

            side = rng.rand<U64>();
        }

        U64 getPsq(Piece pc, Square sq)
        {
            assert(pc >= WHITE_PAWN && pc <= BLACK_KING);
            assert(sq >= a1 && sq <= h8);
            return psq[pc][sq];
        }

        U64 getCastle(int idx)
        {
            assert(idx >= 0 && idx < 16);
            return castle[idx];
        }

        U64 getEp(Square sq)
        {
            assert(sq >= a1 && sq <= h8);
            return ep[fileOf(sq)];
        }

    } // namespace Zobrist

} // namespace Chess
