#include "nnue.h"
#include "accumulator.h"
#include "../chess/misc.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

#include "../incbin.h"

INCBIN(Weights, NNUE_PATH);

namespace NNUE {

    // helper
    int index(Square s, Piece p, Color view) {
        assert(p != NO_PIECE);
        assert(s != NO_SQUARE);

        Color pc = colorOf(p);
        PieceType pt = typeOf(p);
        s = mirrorVertically(view, s);
        const int idx = static_cast<int>(s) + static_cast<int>(pt) * 64 + (pc != view) * 64 * 6;
        return idx;
    }

    // nnue
    void NNUE::init() {
            const unsigned char* data = gWeightsData;
            std::size_t offset = 0;

            memcpy(fc1_weights, data + offset, INPUT_SIZE * HIDDEN_SIZE * sizeof(int16_t));
            offset += INPUT_SIZE * HIDDEN_SIZE * sizeof(int16_t);

            memcpy(fc1_biases, data + offset, HIDDEN_SIZE * sizeof(int16_t));
            offset += HIDDEN_SIZE * sizeof(int16_t);

            memcpy(fc2_weights, data + offset, 2 * HIDDEN_SIZE * sizeof(int16_t));
            offset += 2 * HIDDEN_SIZE * sizeof(int16_t);

            memcpy(fc2_biases, data + offset, OUTPUT_SIZE * sizeof(int32_t));
    }

    int32_t NNUE::forward(const Accumulator &acc, Color stm) const {
        int32_t prediction = fc2_biases[0];

        for (int j = 0; j < HIDDEN_SIZE; j++) {
            if (acc[stm][j] > 0) {
                prediction += fc2_weights[j] * acc[stm][j];
            }

            if (acc[~stm][j] > 0) {
                prediction += fc2_weights[HIDDEN_SIZE + j] * acc[~stm][j];
            }
        }

        return prediction / (512 * 16);
    }

    void NNUE::putPiece(Accumulator &acc, Piece p, Square s) const {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;
            acc[WHITE][i] += fc1_weights[idx + w_idx];
            acc[BLACK][i] += fc1_weights[idx + b_idx];
        }
    }

    void NNUE::removePiece(Accumulator &acc, Piece p, Square s) const {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;
            acc[WHITE][i] -= fc1_weights[idx + w_idx];
            acc[BLACK][i] -= fc1_weights[idx + b_idx];
        }
    }

    void NNUE::movePiece(Accumulator &acc, Piece p, Square from, Square to) const {
        const int w_from_idx = index(from, p, WHITE);
        const int w_to_idx = index(to, p, WHITE);
        const int b_from_idx = index(from, p, BLACK);
        const int b_to_idx = index(to, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++) {
            const int idx = i * INPUT_SIZE;
            acc[WHITE][i] += fc1_weights[idx + w_to_idx] - fc1_weights[idx + w_from_idx];
            acc[BLACK][i] += fc1_weights[idx + b_to_idx] - fc1_weights[idx + b_from_idx];
        }
    }

} // namespace NNUE