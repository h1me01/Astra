#ifndef NNUE_H
#define NNUE_H

#include <fstream>
#include <vector>
#include "../chess/types.h"

using namespace Chess;

namespace NNUE {

    constexpr int NUM_FEATURES = 768;
    constexpr int INPUT_SIZE = 2 * NUM_FEATURES;
    constexpr int HIDDEN_SIZE =  512;
    constexpr int OUTPUT_SIZE = 1;

    class NNUE {
    public:
        int16_t fc1_weights[INPUT_SIZE * HIDDEN_SIZE];
        int16_t fc1_biases[HIDDEN_SIZE];
        int16_t fc2_weights[HIDDEN_SIZE];
        int32_t fc2_biases[OUTPUT_SIZE];

        void init(std::string path);
        int32_t forward(const int16_t *input) const;

        void putPiece(const std::vector<int16_t *> &acc, Piece p, Square s) const;
        void removePiece(const std::vector<int16_t *> &acc, Piece p, Square s) const;
        void movePiece(const std::vector<int16_t *> &acc, Piece p, Square from, Square to) const;

    private:
        void loadParameters(const std::string &filename);
    };

    inline NNUE nnue;

} // namespace NNUE

#endif //NNUE_H
