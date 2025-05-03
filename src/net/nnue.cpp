#include <cstring> // std::memcpy
#include <algorithm>
#include "nnue.h"
#include "accumulator.h"
#include "../chess/board.h"

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

    // helper

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

    int featureIndex(Square psq, Square ksq, Piece pc, Color view)
    {
        assert(psq >= a1 && psq <= h8);
        assert(ksq >= a1 && ksq <= h8);
        assert(view == WHITE || view == BLACK);
        assert(pc >= WHITE_PAWN && pc <= BLACK_KING);

        // mirror psq horizontally if king is on other half
        if (fileOf(ksq) > 3)
            psq = Square(psq ^ 7);

        psq = relSquare(view, psq);
        ksq = relSquare(view, ksq);

        return psq + typeOf(pc) * 64 + (colorOf(pc) != view) * 64 * 6 + KING_BUCKET[ksq] * FEATURE_SIZE;
    }

    // nnue
    void NNUE::init()
    {
        std::size_t offset = 0;

        memcpy(ft_weights, &gWeightsData[offset], INPUT_SIZE * FT_SIZE * sizeof(int16_t));
        offset += INPUT_SIZE * FT_SIZE * sizeof(int16_t);

        memcpy(ft_biases, &gWeightsData[offset], FT_SIZE * sizeof(int16_t));
        offset += FT_SIZE * sizeof(int16_t);

        memcpy(l1_weights, &gWeightsData[offset], 2 * FT_SIZE * sizeof(int16_t));
        offset += 2 * FT_SIZE * sizeof(int16_t);

        memcpy(l1_biases, &gWeightsData[offset], L1_SIZE * sizeof(int16_t));
    }

    void NNUE::initAccum(Accum &acc) const
    {
        memcpy(acc.getData(WHITE), nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
        memcpy(acc.getData(BLACK), nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
    }

    int32_t NNUE::forward(Board &board) const
    {
        Color stm = board.getTurn();
        Accum &acc = board.getAccumulator();

        assert(stm == WHITE || stm == BLACK);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        constexpr avx_type zero{};
        const avx_type max_clipped_value = avx_set1_epi16(CRELU_CLIP);

        const auto acc_stm = (const avx_type *)acc.getData(stm);
        const auto acc_opp = (const avx_type *)acc.getData(~stm);

        avx_type res{};
        const auto weights = (const avx_type *)(l1_weights);

        for (int i = 0; i < FT_SIZE / div; i++)
        {
            auto clipped_stm = avx_min_epi16(avx_max_epi16(acc_stm[i], zero), max_clipped_value);
            res = avx_add_epi32(res, avx_madd_epi16(weights[i], clipped_stm));
        }

        for (int i = 0; i < FT_SIZE / div; i++)
        {
            auto clipped_opp = avx_min_epi16(avx_max_epi16(acc_opp[i], zero), max_clipped_value);
            res = avx_add_epi32(res, avx_madd_epi16(weights[i + FT_SIZE / div], clipped_opp));
        }

        return horizontalSum(res) / (FT_QUANT * L1_QUANT) + l1_biases[0] / L1_QUANT;
#else
        int32_t output = 0;

        int16_t *acc_stm = acc.getData(stm);
        int16_t *acc_opp = acc.getData(~stm);

        for (int j = 0; j < HIDDEN_SIZE; j++)
        {
            output += l1_weights[j] * std::clamp(int32_t(acc_stm[j]), 0, CRELU_CLIP);
            output += l1_weights[HIDDEN_SIZE + j] * std::clamp(int32_t(acc_opp[j]), 0, CRELU_CLIP);
        }

        return output / (FT_QUANT * L1_QUANT) + l1_biases[0] / L1_QUANT;
#endif
    }

    void NNUE::putPiece(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const
    {
        const int idx = featureIndex(psq, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_data = (avx_type *)acc.getData(view);
        const avx_type *prev_data = acc.isInitialized(view) ? acc_data : (avx_type *)prev.getData(view);

        const auto weights = (const avx_type *)(ft_weights + idx * FT_SIZE);
        for (int i = 0; i < FT_SIZE / div; i++)
            acc_data[i] = avx_add_epi16(prev_data[i], weights[i]);
#else
        int16_t *acc_view = acc.getData(view);
        int16_t *prev_view = acc.isInitialized(view) ? acc_view : prev.getData(view);

        for (int i = 0; i < HIDDEN_SIZE; i++)
            acc_view[i] = prev_view[i] + ft_weights[idx * HIDDEN_SIZE + i];
#endif

        acc.markAsInitialized(view);
    }

    void NNUE::removePiece(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const
    {
        const int idx = featureIndex(psq, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_data = (avx_type *)acc.getData(view);
        const avx_type *prev_data = acc.isInitialized(view) ? acc_data : (avx_type *)prev.getData(view);

        const auto weights = (const avx_type *)(ft_weights + idx * FT_SIZE);
        for (int i = 0; i < FT_SIZE / div; i++)
            acc_data[i] = avx_sub_epi16(prev_data[i], weights[i]);
#else
        int16_t *acc_view = acc.getData(view);
        int16_t *prev_view = acc.isInitialized(view) ? acc_view : prev.getData(view);

        for (int i = 0; i < HIDDEN_SIZE; i++)
            acc_view[i] = prev_view[i] - ft_weights[idx * HIDDEN_SIZE + i];
#endif

        acc.markAsInitialized(view);
    }

    void NNUE::movePiece(Accum &acc, Accum &prev, Piece pc, Square from, Square to, Square ksq, Color view) const
    {
        const int from_idx = featureIndex(from, ksq, pc, view);
        const int to_idx = featureIndex(to, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
        avx_type *acc_data = (avx_type *)acc.getData(view);
        const avx_type *prev_data = acc.isInitialized(view) ? acc_data : (avx_type *)prev.getData(view);

        const auto weights_from = (const avx_type *)(ft_weights + from_idx * FT_SIZE);
        const auto weights_to = (const avx_type *)(ft_weights + to_idx * FT_SIZE);

        for (int i = 0; i < FT_SIZE / div; i++)
            acc_data[i] = avx_add_epi16(prev_data[i], avx_sub_epi16(weights_to[i], weights_from[i]));
#else
        int16_t *acc_view = acc.getData(view);
        int16_t *prev_view = acc.isInitialized(view) ? acc_view : prev.getData(view);

        for (int i = 0; i < HIDDEN_SIZE; i++)
        {
            int16_t diff = ft_weights[to_idx * HIDDEN_SIZE + i] - ft_weights[from_idx * HIDDEN_SIZE + i];
            acc_view[i] = prev_view[i] + diff;
        }
#endif

        acc.markAsInitialized(view);
    }

    // global variable
    NNUE nnue;

} // namespace NNUE
