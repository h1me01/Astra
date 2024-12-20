#include <cstring> // std::memcpy
#include "nnue.h"
#include "../chess/misc.h"

#include "incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

#if defined(__AVX512F__)
using avx_type = __m512i;
#define div (32)
#define avx_madd_epi16 _mm512_madd_epi16
#define avx_add_epi32 _mm512_add_epi32
#define avx_add_epi16 _mm512_add_epi16
#define avx_sub_epi16 _mm512_sub_epi16
#define avx_max_epi16 _mm512_max_epi16
#elif defined(__AVX2__) || defined(__AVX__)
using avx_type = __m256i;
#define div (16)
#define avx_madd_epi16 _mm256_madd_epi16
#define avx_add_epi32 _mm256_add_epi32
#define avx_add_epi16 _mm256_add_epi16
#define avx_sub_epi16 _mm256_sub_epi16
#define avx_max_epi16 _mm256_max_epi16
#endif

namespace NNUE
{
#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    inline int32_t sumRegisterEpi32(avx_type &reg)
    {
#if defined(__AVX512F__)
        const __m256i red_8 = _mm256_add_epi32(_mm512_castsi512_si256(reg), _mm512_extracti32x8_epi32(reg, 1));
#elif defined(__AVX2__) || defined(__AVX__)
        const __m256i red_8 = reg;
#endif
        const __m128i red_4 = _mm_add_epi32(_mm256_castsi256_si128(red_8), _mm256_extractf128_si256(red_8, 1));

        __m128i vsum = _mm_add_epi32(red_4, _mm_srli_si128(red_4, 8));
        vsum = _mm_add_epi32(vsum, _mm_srli_si128(vsum, 4));
        int32_t sums = _mm_cvtsi128_si32(vsum);

        return sums;
    }
#endif

    // helper
    int index(Square s, Piece p, Color view)
    {
        assert(p != NO_PIECE);
        assert(s != NO_SQUARE);

        if (view == BLACK)
            s = Square(s ^ 56);

        return s + typeOf(p) * 64 + (colorOf(p) != view) * 64 * 6;
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
#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        constexpr avx_type zero{};

        const auto acc_stm = (const avx_type *)acc.data[stm];
        const auto acc_opp = (const avx_type *)acc.data[~stm];

        avx_type res{};
        const auto weights = (const avx_type *)(fc2_weights);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            res = avx_add_epi32(res, avx_madd_epi16(avx_max_epi16(acc_stm[i], zero), weights[i]));
        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            res = avx_add_epi32(res, avx_madd_epi16(avx_max_epi16(acc_opp[i], zero), weights[i + HIDDEN_SIZE / div]));

        const auto output = sumRegisterEpi32(res) + fc2_biases[0];
        return output / 512 / 16;
#else
        int32_t output = fc2_biases[0];

        for (int j = 0; j < HIDDEN_SIZE; j++)
        {
            if (acc.data[stm][j] > 0)
                output += fc2_weights[j] * acc.data[stm][j];

            if (acc.data[~stm][j] > 0)
                output += fc2_weights[HIDDEN_SIZE + j] * acc.data[~stm][j];
        }

        return output / 512 / 16;
#endif
    }

    void NNUE::putPiece(Accumulator &acc, Piece p, Square s) const
    {
        const int w_idx = index(s, p, WHITE);
        const int b_idx = index(s, p, BLACK);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_white = (avx_type *)acc.data[WHITE];
        avx_type *acc_black = (avx_type *)acc.data[BLACK];

        const auto wgt_white = (const avx_type *)(fc1_weights + w_idx * HIDDEN_SIZE);
        const auto wgt_black = (const avx_type *)(fc1_weights + b_idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_white[i] = avx_add_epi16(acc_white[i], wgt_white[i]);
        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_black[i] = avx_add_epi16(acc_black[i], wgt_black[i]);
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

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_white = (avx_type *)acc.data[WHITE];
        avx_type *acc_black = (avx_type *)acc.data[BLACK];

        const auto wgt_white = (const avx_type *)(fc1_weights + w_idx * HIDDEN_SIZE);
        const auto wgt_black = (const avx_type *)(fc1_weights + b_idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_white[i] = avx_sub_epi16(acc_white[i], wgt_white[i]);
        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_black[i] = avx_sub_epi16(acc_black[i], wgt_black[i]);
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

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_white = (avx_type *)acc.data[WHITE];
        avx_type *acc_black = (avx_type *)acc.data[BLACK];

        const auto wgt_white_from = (const avx_type *)(fc1_weights + w_from_idx * HIDDEN_SIZE);
        const auto wgt_white_to = (const avx_type *)(fc1_weights + w_to_idx * HIDDEN_SIZE);
        const auto wgt_black_from = (const avx_type *)(fc1_weights + b_from_idx * HIDDEN_SIZE);
        const auto wgt_black_to = (const avx_type *)(fc1_weights + b_to_idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_white[i] = avx_add_epi16(acc_white[i], avx_sub_epi16(wgt_white_to[i], wgt_white_from[i]));
        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_black[i] = avx_add_epi16(acc_black[i], avx_sub_epi16(wgt_black_to[i], wgt_black_from[i]));
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
