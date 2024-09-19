#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"

namespace Chess {

    // psuedorandom number generator from stockfish
    class PRNG {
        U64 s;

        U64 rand64() {
            s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
            return s * 2685821657736338717LL;
        }

    public:
        explicit PRNG(U64 seed) : s(seed) {}

        // generate psuedorandom number
        template<typename T>
        T rand() { return T(rand64()); }

        // generate psuedorandom number with only a few setFen bits
        template<typename T>
        T sparse_rand() {
            return T(rand64() & rand64() & rand64());
        }
    };

    namespace Zobrist {
        // used to incrementally update the hash key of a position
        inline U64 psq[NUM_PIECES][NUM_SQUARES];
        inline U64 castle[16];
        inline U64 ep[8];
        inline U64 side;

        inline void init() {
            PRNG rng(70026072);

            for (auto & p : psq)
                for (U64 & s : p)
                    s = rng.rand<U64>();

            for(U64 & f : ep)
                f = rng.rand<U64>();

            for(U64 & cr : castle)
                cr = rng.rand<U64>();

            side = rng.rand<U64>();
        }
    } // namespace zobrist

} // namespace Chess

#endif //ZOBRIST_H
