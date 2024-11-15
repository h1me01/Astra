#ifndef NNUE_H
#define NNUE_H

#include <array>
#include <cassert>
#include "../chess/types.h"

using namespace Chess;

#if defined(AVX2)
    #define ALIGNMENT 256
#else
    #define ALIGNMENT 32
#endif

namespace NNUE
{
    constexpr int INPUT_SIZE = 768;
    constexpr int HIDDEN_SIZE = 1024;
    constexpr int OUTPUT_SIZE = 1;

    struct Accumulator {
       alignas(ALIGNMENT) int16_t data[2][HIDDEN_SIZE]; 
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
            assert(index < MAX_PLY);
            accumulators[index] = accumulators[index - 1];
        }

        void pop()
        {
            assert(index > 0);
            index--;
        }

        Accumulator& back() { return accumulators[index]; }

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

        int32_t forward(const Accumulator& acc, Color stm) const;

        void putPiece(Accumulator& acc, Piece p, Square s) const;
        void removePiece(Accumulator& acc, Piece p, Square s) const;
        void movePiece(Accumulator& acc, Piece p, Square from, Square to) const;
    };

    // global variable
    extern NNUE nnue;

} // namespace NNUE

#endif //NNUE_H
