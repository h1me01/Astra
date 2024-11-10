#include <cstring> // std::memcpy
#include "nnue.h"
#include "../chess/misc.h"

#include "incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

namespace NNUE
{
    // helper
    int index(Square s, Piece p, Color view)
    {
        assert(p != NO_PIECE);
        assert(s != NO_SQUARE);

        if (view == BLACK)
            s = Square(s ^ 56);

        return int(s) + typeOf(p) * 64 + (colorOf(p) != view) * 64 * 6;
    }

    // nnue
    void NNUE::init()
    {
        std::size_t offset = 0;

        memcpy(fc1_weights, &gWeightsData[offset], INPUT_SIZE * HIDDEN_SIZE * sizeof(int16_t));
        offset += INPUT_SIZE * HIDDEN_SIZE * sizeof(int16_t);

        memcpy(fc1_biases, &gWeightsData[offset], HIDDEN_SIZE * sizeof(int16_t));
        offset += HIDDEN_SIZE * sizeof(int16_t);

        memcpy(fc2_weights, &gWeightsData[offset], 2 * HIDDEN_SIZE * sizeof(int16_t));
        offset += 2 * HIDDEN_SIZE * sizeof(int16_t);

        memcpy(fc2_biases, &gWeightsData[offset], OUTPUT_SIZE * sizeof(int32_t));
    }

    int32_t NNUE::forward(const Accumulator& acc, Color stm) const
    {
        int32_t prediction = fc2_biases[0];

        for (int j = 0; j < HIDDEN_SIZE; j++)
        {
            if (acc[stm][j] > 0)
                prediction += fc2_weights[j] * acc[stm][j];

            if (acc[~stm][j] > 0)
                prediction += fc2_weights[HIDDEN_SIZE + j] * acc[~stm][j];
        }

        return prediction / (512 * 16);
    }

    void NNUE::putPiece(Accumulator& acc, Piece p, Square s) const
    {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            acc[WHITE][i] += fc1_weights[w_idx * HIDDEN_SIZE + i];
            acc[BLACK][i] += fc1_weights[b_idx * HIDDEN_SIZE + i];
        }
    }

    void NNUE::removePiece(Accumulator& acc, Piece p, Square s) const
    {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            acc[WHITE][i] -= fc1_weights[w_idx * HIDDEN_SIZE + i];
            acc[BLACK][i] -= fc1_weights[b_idx * HIDDEN_SIZE + i];
        }
    }

    void NNUE::movePiece(Accumulator& acc, Piece p, Square from, Square to) const
    {
        const int w_from_idx = index(from, p, WHITE);
        const int w_to_idx = index(to, p, WHITE);
        const int b_from_idx = index(from, p, BLACK);
        const int b_to_idx = index(to, p, BLACK);

        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            acc[WHITE][i] += fc1_weights[w_to_idx * HIDDEN_SIZE + i] - fc1_weights[w_from_idx * HIDDEN_SIZE + i];
            acc[BLACK][i] += fc1_weights[b_to_idx * HIDDEN_SIZE + i] - fc1_weights[b_from_idx * HIDDEN_SIZE + i];
        }
    }

    // global variable
    NNUE nnue;

} // namespace NNUE
