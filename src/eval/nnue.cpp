#include <cstring> // std::memcpy
#include <immintrin.h>
#include "nnue.h"
#include "../chess/misc.h"

#include "incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

#define avx_madd_epi16 _mm256_madd_epi16
#define avx_add_epi32 _mm256_add_epi32
#define avx_add_epi16 _mm256_add_epi16
#define avx_sub_epi16 _mm256_sub_epi16
#define avx_max_epi16 _mm256_max_epi16

namespace NNUE
{
    // simd
    inline int32_t sumRegisterEpi32(__m256i &reg)
    {
        const __m256i reduced_8 = reg;
        const __m128i reduced_4 = _mm_add_epi32(_mm256_castsi256_si128(reduced_8), _mm256_extractf128_si256(reduced_8, 1));

        // summarize the 128 register using SSE instructions
        __m128i vsum = _mm_add_epi32(reduced_4, _mm_srli_si128(reduced_4, 8));
        vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
        int32_t sums = _mm_cvtsi128_si32(vsum);
        return sums;
    }

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

    int32_t NNUE::forward(const Accumulator &acc, Color stm) const
    {
#if defined(AVX2)
        __m256i reluBias = _mm256_setzero_si256();

        const auto acc_stm = (__m256i *)acc.data[stm];
        const auto acc_opp = (__m256i *)acc.data[~stm];

        __m256i res = _mm256_setzero_si256();
        const __m256i *weights = (const __m256i *)(fc2_weights);

        for (int i = 0; i < HIDDEN_SIZE / 16; i++)
        {
            __m256i acc_act_data = avx_max_epi16(acc_stm[i], reluBias);
            __m256i acc_nac_data = avx_max_epi16(acc_opp[i], reluBias);
            res = avx_add_epi32(res, avx_madd_epi16(acc_act_data, weights[i]));
            res = avx_add_epi32(res, avx_madd_epi16(acc_nac_data, weights[i + HIDDEN_SIZE / 16]));
        }

        const auto outp = sumRegisterEpi32(res) + fc2_biases[0];
        return outp / 512 / 16;
#else
        int32_t prediction = fc2_biases[0];

        for (int j = 0; j < HIDDEN_SIZE; j++)
        {
            if (acc.data[stm][j] > 0)
                prediction += fc2_weights[j] * acc.data[stm][j];

            if (acc.data[~stm][j] > 0)
                prediction += fc2_weights[HIDDEN_SIZE + j] * acc.data[~stm][j];
        }
        return prediction / 512 / 16;
#endif
    }

    void NNUE::putPiece(Accumulator &acc, Piece p, Square s) const
    {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

#if defined(AVX2)
        __m256i *acc_white = (__m256i *)acc.data[WHITE];
        __m256i *acc_black = (__m256i *)acc.data[BLACK];

        const __m256i *wgt_white = (const __m256i *)(fc1_weights + w_idx * HIDDEN_SIZE);
        const __m256i *wgt_black = (const __m256i *)(fc1_weights + b_idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / 16; i++)
        {
            acc_white[i] = avx_add_epi16(acc_white[i], wgt_white[i]);
            acc_black[i] = avx_add_epi16(acc_black[i], wgt_black[i]);
        }
#else
        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            acc.data[WHITE][i] += fc1_weights[w_idx * HIDDEN_SIZE + i];
            acc.data[BLACK][i] += fc1_weights[b_idx * HIDDEN_SIZE + i];
        }
#endif
    }

    void NNUE::removePiece(Accumulator &acc, Piece p, Square s) const
    {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

#if defined(AVX2)
        __m256i *acc_white = (__m256i *)acc.data[WHITE];
        __m256i *acc_black = (__m256i *)acc.data[BLACK];

        const __m256i *wgt_white = (const __m256i *)(fc1_weights + w_idx * HIDDEN_SIZE);
        const __m256i *wgt_black = (const __m256i *)(fc1_weights + b_idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / 16; i++)
        {
            acc_white[i] = avx_sub_epi16(acc_white[i], wgt_white[i]);
            acc_black[i] = avx_sub_epi16(acc_black[i], wgt_black[i]);
        }
#else
        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            acc.data[WHITE][i] -= fc1_weights[w_idx * HIDDEN_SIZE + i];
            acc.data[BLACK][i] -= fc1_weights[b_idx * HIDDEN_SIZE + i];
        }
#endif
    }

    void NNUE::movePiece(Accumulator &acc, Piece p, Square from, Square to) const
    {
        const int w_from_idx = index(from, p, WHITE);
        const int w_to_idx = index(to, p, WHITE);
        const int b_from_idx = index(from, p, BLACK);
        const int b_to_idx = index(to, p, BLACK);

#if defined(AVX2)
        __m256i *acc_white = (__m256i *)acc.data[WHITE];
        __m256i *acc_black = (__m256i *)acc.data[BLACK];

        const __m256i *wgt_white_from = (const __m256i *)(fc1_weights + w_from_idx * HIDDEN_SIZE);
        const __m256i *wgt_white_to = (const __m256i *)(fc1_weights + w_to_idx * HIDDEN_SIZE);
        const __m256i *wgt_black_from = (const __m256i *)(fc1_weights + b_from_idx * HIDDEN_SIZE);
        const __m256i *wgt_black_to = (const __m256i *)(fc1_weights + b_to_idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / 16; i++)
        {
            acc_white[i] = avx_add_epi16(acc_white[i], avx_sub_epi16(wgt_white_to[i], wgt_white_from[i]));
            acc_black[i] = avx_add_epi16(acc_black[i], avx_sub_epi16(wgt_black_to[i], wgt_black_from[i]));
        }
#else
        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            acc.data[WHITE][i] += fc1_weights[w_to_idx * HIDDEN_SIZE + i] - fc1_weights[w_from_idx * HIDDEN_SIZE + i];
            acc.data[BLACK][i] += fc1_weights[b_to_idx * HIDDEN_SIZE + i] - fc1_weights[b_from_idx * HIDDEN_SIZE + i];
        }
#endif
    }
    // global variable
    NNUE nnue;

} // namespace NNUE
