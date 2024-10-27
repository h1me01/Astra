#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"

namespace Chess
{
    // psuedorandom number generator from stockfish
    class PRNG
    {
        U64 s;

        U64 rand64()
        {
            s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
            return s * 2685821657736338717LL;
        }

    public:
        explicit PRNG(U64 seed) : s(seed) {}

        // generate psuedorandom number
        template <typename T>
        T rand() { return T(rand64()); }

        // generate psuedorandom number with only a few setFen bits
        template <typename T>
        T sparse_rand() { return T(rand64() & rand64() & rand64()); }
    };

    namespace Zobrist
    {
        extern U64 psq[NUM_PIECES][NUM_SQUARES];
        extern U64 castle[16];
        extern U64 ep[8];
        extern U64 side;

        void init();
    } // namespace Zobrist

} // namespace Chess

#endif //ZOBRIST_H
