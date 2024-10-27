#include "zobrist.h"

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
            PRNG rng(70026072);

            for (auto& p : psq)
                for (U64& s : p)
                    s = rng.rand<U64>();

            for (U64& f : ep)
                f = rng.rand<U64>();

            for (U64& cr : castle)
                cr = rng.rand<U64>();

            side = rng.rand<U64>();
        }
    } // namespace Zobrist

} // namespace Chess
