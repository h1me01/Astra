#ifndef NNUE_H
#define NNUE_H

#include <array>
#include "../chess/types.h"

using namespace Chess;

namespace NNUE {

    constexpr int INPUT_SIZE = 768;
    constexpr int HIDDEN_SIZE = 512;
    constexpr int OUTPUT_SIZE = 1;

    using Accumulator = std::array<std::array<int16_t, HIDDEN_SIZE>, 2>;

    struct NNUE {
        int16_t fc1_weights[INPUT_SIZE * HIDDEN_SIZE];
        int16_t fc1_biases[HIDDEN_SIZE];
        int16_t fc2_weights[2 * HIDDEN_SIZE];
        int32_t fc2_biases[OUTPUT_SIZE];

        void init();

        int32_t forward(const Accumulator &acc, Color stm) const;

        void putPiece(Accumulator &acc, Piece p, Square s) const;
        void removePiece(Accumulator &acc, Piece p, Square s) const;
        void movePiece(Accumulator &acc, Piece p, Square from, Square to) const;
    };

    inline NNUE nnue;

} // namespace NNUE

#endif //NNUE_H
