#include <cstring> // std::memcpy
#include <algorithm>

#include "nnue.h"
#include "accumulator.h"
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
#define avx_min_epi16 _mm512_min_epi16
#define avx_set1_epi16 _mm512_set1_epi16

#elif defined(__AVX2__) || defined(__AVX__)
using avx_type = __m256i;
#define div (16)
#define avx_madd_epi16 _mm256_madd_epi16
#define avx_add_epi32 _mm256_add_epi32
#define avx_add_epi16 _mm256_add_epi16
#define avx_sub_epi16 _mm256_sub_epi16
#define avx_max_epi16 _mm256_max_epi16
#define avx_min_epi16 _mm256_min_epi16
#define avx_set1_epi16 _mm256_set1_epi16

#endif

namespace NNUE
{
#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    int32_t horizontalSum(avx_type &reg)
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
    int index(Square psq, Square ksq, Piece p, Color view)
    {
        assert(p != NO_PIECE);
        assert(psq != NO_SQUARE);

        // mirror psq horizontally if king is on other half
        if (fileOf(ksq) > 3)
            psq = Square(psq ^ 7);

        psq = relativeSquare(view, psq);
        ksq = relativeSquare(view, ksq);

        return psq + typeOf(p) * 64 + (colorOf(p) != view) * 64 * 6 + KING_BUCKET[ksq] * FEATURE_SIZE;
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
        const avx_type max_clipped_value = avx_set1_epi16(127 * 32);

        const auto acc_stm = (const avx_type *)acc.data[stm];
        const auto acc_opp = (const avx_type *)acc.data[~stm];

        avx_type res{};
        const auto weights = (const avx_type *)(fc2_weights);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
        {
            auto clipped_stm = avx_min_epi16(avx_max_epi16(acc_stm[i], zero), max_clipped_value);
            res = avx_add_epi32(res, avx_madd_epi16(weights[i], clipped_stm));
        }

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
        {
            auto clipped_opp = avx_min_epi16(avx_max_epi16(acc_opp[i], zero), max_clipped_value);
            res = avx_add_epi32(res, avx_madd_epi16(weights[i + HIDDEN_SIZE / div], clipped_opp));
        }

        const auto output = horizontalSum(res) + fc2_biases[0];

        return output / 128 / 32;
#else
        int32_t output = fc2_biases[0];

        for (int j = 0; j < HIDDEN_SIZE; j++)
        {
            output += fc2_weights[j] * std::clamp(int32_t(acc.data[stm][j]), 0, 32 * 127);
            output += fc2_weights[HIDDEN_SIZE + j] * std::clamp(int32_t(acc.data[~stm][j]), 0, 32 * 127);
        }

        return output / 128 / 32;
#endif
    }

    void NNUE::putPiece(Accumulator &acc, Piece pc, Square psq, Square ksq, Color view) const
    {
        const int idx = index(psq, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_view = (avx_type *)acc.data[view];
        const auto weights = (const avx_type *)(fc1_weights + idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_view[i] = avx_add_epi16(acc_view[i], weights[i]);
#else
        for (int i = 0; i < HIDDEN_SIZE; i++)
            acc.data[view][i] += fc1_weights[idx * HIDDEN_SIZE + i];
#endif
    }

    void NNUE::putPiece(Accumulator &acc, Piece pc, Square psq, Square wksq, Square bksq) const
    {
        putPiece(acc, pc, psq, wksq, WHITE);
        putPiece(acc, pc, psq, bksq, BLACK);
    }

    void NNUE::removePiece(Accumulator &acc, Piece pc, Square psq, Square ksq, Color view) const
    {
        const int idx = index(psq, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_view = (avx_type *)acc.data[view];
        const auto weights = (const avx_type *)(fc1_weights + idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_view[i] = avx_sub_epi16(acc_view[i], weights[i]);
#else
        for (int i = 0; i < HIDDEN_SIZE; i++)
            acc.data[view][i] -= fc1_weights[idx * HIDDEN_SIZE + i];
#endif
    }

    void NNUE::removePiece(Accumulator &acc, Piece pc, Square psq, Square wksq, Square bksq) const
    {
        removePiece(acc, pc, psq, wksq, WHITE);
        removePiece(acc, pc, psq, bksq, BLACK);
    }

    void NNUE::movePiece(Accumulator &acc, Piece pc, Square from, Square to, Square ksq, Color view) const
    {
        const int from_idx = index(from, ksq, pc, view);
        const int to_idx = index(to, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_view = (avx_type *)acc.data[view];

        const auto weights_from = (const avx_type *)(fc1_weights + from_idx * HIDDEN_SIZE);
        const auto weights_to = (const avx_type *)(fc1_weights + to_idx * HIDDEN_SIZE);

        for (int i = 0; i < HIDDEN_SIZE / div; i++)
            acc_view[i] = avx_add_epi16(acc_view[i], avx_sub_epi16(weights_to[i], weights_from[i]));
#else
        for (int i = 0; i < HIDDEN_SIZE; i++)
            acc.data[view][i] += fc1_weights[to_idx * HIDDEN_SIZE + i] - fc1_weights[from_idx * HIDDEN_SIZE + i];
#endif
    }

    void NNUE::movePiece(Accumulator &acc, Piece pc, Square from, Square to, Square wksq, Square bksq) const
    {
        movePiece(acc, pc, from, to, wksq, WHITE);
        movePiece(acc, pc, from, to, bksq, BLACK);
    }

    // global variable
    NNUE nnue;

} // namespace NNUE
