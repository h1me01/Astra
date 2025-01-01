#ifndef NNUE_H
#define NNUE_H

#include <array>
#include <cassert>
#include <immintrin.h>
#include "../chess/types.h"

#include <iostream>

using namespace Chess;

#if defined(__AVX512F__)
#define ALIGNMENT 64
#else
#define ALIGNMENT 32
#endif

#include <cstring>

namespace NNUE
{
    constexpr int BUCKET_SIZE = 10;
    constexpr int FEATURE_SIZE = 768;

    constexpr int INPUT_SIZE = BUCKET_SIZE * FEATURE_SIZE;
    constexpr int HIDDEN_SIZE = 1024;
    constexpr int OUTPUT_SIZE = 1;

    // clang-format off
    constexpr int KING_BUCKET[NUM_SQUARES] {
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

    struct Accumulator
    {
        alignas(ALIGNMENT) int16_t data[NUM_COLORS][HIDDEN_SIZE];
    };

    class Accumulators
    {
    public:
        Accumulators() : index(0) {}

        int size() const { return index; }

        void clear() { index = 0; }

        void push()
        {
            index++;
            assert(index < MAX_PLY + 1);
            accumulators[index] = accumulators[index - 1];
        }

        void pop()
        {
            assert(index > 0);
            index--;
        }

        Accumulator &back() { return accumulators[index]; }

    private:
        int index;
        std::array<Accumulator, MAX_PLY + 1> accumulators{};
    };

    struct NNUE
    {
        alignas(ALIGNMENT) int16_t fc1_weights[INPUT_SIZE * HIDDEN_SIZE];
        alignas(ALIGNMENT) int16_t fc1_biases[HIDDEN_SIZE];
        alignas(ALIGNMENT) int16_t fc2_weights[2 * HIDDEN_SIZE];
        alignas(ALIGNMENT) int32_t fc2_biases[OUTPUT_SIZE];

        void init();
        
        int32_t forward(const Accumulator &acc, Color stm) const;
        void putPiece(Accumulator &acc, Piece pc, Square psq, Square wksq, Square bksq) const;
        void removePiece(Accumulator &acc, Piece pc, Square psq, Square wksq, Square bksq) const;
        void movePiece(Accumulator &acc, Piece pc, Square from, Square to, Square wksq, Square bksq) const;
    };

    // global variable
    extern NNUE nnue;

} // namespace NNUE

#endif // NNUE_H
