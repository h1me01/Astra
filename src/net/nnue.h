#pragma once

#include <array>
#include <cassert>
#include <immintrin.h>
#include <cstring>
#include "../chess/misc.h"

#if defined(__AVX512F__)
#define ALIGNMENT 64
#else
#define ALIGNMENT 32
#endif

using namespace Chess;

namespace Chess
{
    class Board;
}

namespace NNUE
{
    class Accum;

    // 2x(10x768->1024)->1

    constexpr int BUCKET_SIZE = 10;
    constexpr int FEATURE_SIZE = 768;

    constexpr int INPUT_SIZE = BUCKET_SIZE * FEATURE_SIZE;
    constexpr int HIDDEN_SIZE = 1024;
    constexpr int OUTPUT_SIZE = 1;

    constexpr int FT_QUANT = 32;
    constexpr int L1_QUANT = 128;

    constexpr int CRELU_CLIP = FT_QUANT * 127;

    // clang-format off
    constexpr int KING_BUCKET[NUM_SQUARES]
    {
        0, 1, 2, 3, 3, 2, 1, 0,
        4, 4, 5, 5, 5, 5, 4, 4,
        6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7,
        8, 8, 8, 8, 8, 8, 8, 8,
        8, 8, 8, 8, 8, 8, 8, 8,
        9, 9, 9, 9, 9, 9, 9, 9,
        9, 9, 9, 9, 9, 9, 9, 9,
    };
    // clang-format on

    inline bool needsRefresh(Piece pc, Square from, Square to)
    {
        if (typeOf(pc) != KING)
            return false;

        Color view = colorOf(pc);
        return KING_BUCKET[relSquare(view, from)] != KING_BUCKET[relSquare(view, to)] || fileOf(from) + fileOf(to) == 7;
    }

    class NNUE
    {
    private:
        alignas(ALIGNMENT) int16_t ft_weights[INPUT_SIZE * HIDDEN_SIZE];
        alignas(ALIGNMENT) int16_t ft_biases[HIDDEN_SIZE];
        alignas(ALIGNMENT) int16_t l1_weights[2 * HIDDEN_SIZE];
        alignas(ALIGNMENT) int16_t l1_biases[OUTPUT_SIZE];

    public:
        void init();
        void initAccum(Accum &acc) const;

        int32_t forward(Board &board) const;

        void putPiece(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const;
        void removePiece(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const;
        void movePiece(Accum &acc, Accum &prev, Piece pc, Square from, Square to, Square ksq, Color view) const;
    };

    // global variable
    extern NNUE nnue;

} // namespace NNUE
