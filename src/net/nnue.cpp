#include <algorithm>
#include <cstring> // std::memcpy

#include "../chess/board.h"
#include "accumulator.h"
#include "nnue.h"

#include "incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

#if defined(__AVX512F__)

using avx_type = __m512i;

#define div (32)
#define madd_epi16 _mm512_madd_epi16
#define add_epi32 _mm512_add_epi32
#define add_epi16 _mm512_add_epi16
#define sub_epi16 _mm512_sub_epi16
#define max_epi16 _mm512_max_epi16
#define min_epi16 _mm512_min_epi16
#define set1_epi16 _mm512_set1_epi16
#define mullo_epi16 _mm512_mullo_epi16

#elif defined(__AVX2__) || defined(__AVX__)

using avx_type = __m256i;

#define div (16)
#define madd_epi16 _mm256_madd_epi16
#define add_epi32 _mm256_add_epi32
#define add_epi16 _mm256_add_epi16
#define sub_epi16 _mm256_sub_epi16
#define max_epi16 _mm256_max_epi16
#define min_epi16 _mm256_min_epi16
#define set1_epi16 _mm256_set1_epi16
#define mullo_epi16 _mm256_mullo_epi16

#endif

namespace NNUE {

// helper

int32_t fma(const int16_t input, const int16_t weight) {
    int32_t clipped = std::clamp(int32_t(input), 0, FT_QUANT);
    return weight * clipped * clipped;
}

avx_type fma(const avx_type &input, const avx_type &weight) {
    const avx_type zero{};
    const avx_type ft_clip = set1_epi16(FT_QUANT);

    auto act = min_epi16(max_epi16(input, zero), ft_clip);
    auto product = madd_epi16(mullo_epi16(act, weight), act);

    return product;
}

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
int32_t hor_sum(avx_type &reg) {
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

int feature_idx(Square psq, Square ksq, Piece pc, Color view) {
    assert(valid_sq(psq));
    assert(valid_sq(ksq));
    assert(valid_color(view));
    assert(valid_piece(pc));

    // mirror psq horizontally if king is on other half
    if(sq_file(ksq) > 3)
        psq = Square(psq ^ 7);

    psq = rel_sq(view, psq);
    ksq = rel_sq(view, ksq);

    return psq + piece_type(pc) * 64 + (piece_color(pc) != view) * 64 * 6 + KING_BUCKET[ksq] * FEATURE_SIZE;
}

// nnue
void NNUE::init() {
    std::size_t offset = 0;

    memcpy(ft_weights, &gWeightsData[offset], FT_SIZE * L1_SIZE * sizeof(int16_t));
    offset += FT_SIZE * L1_SIZE * sizeof(int16_t);

    memcpy(ft_biases, &gWeightsData[offset], L1_SIZE * sizeof(int16_t));
    offset += L1_SIZE * sizeof(int16_t);

    memcpy(l1_weights, &gWeightsData[offset], 2 * L1_SIZE * OUTPUT_BUCKETS * sizeof(int16_t));
    offset += 2 * L1_SIZE * OUTPUT_BUCKETS * sizeof(int16_t);

    memcpy(l1_biases, &gWeightsData[offset], OUTPUT_BUCKETS * sizeof(int16_t));
}

void NNUE::init_accum(Accum &acc) const {
    memcpy(acc.get_data(WHITE), nnue.ft_biases, sizeof(int16_t) * L1_SIZE);
    memcpy(acc.get_data(BLACK), nnue.ft_biases, sizeof(int16_t) * L1_SIZE);
}

int32_t NNUE::forward(Board &board) const {
    board.update_accums();

    const int bucket = (pop_count(board.occupancy()) - 2) / 4;
    assert(0 <= bucket && bucket < OUTPUT_BUCKETS);

    Color stm = board.get_stm();
    Accum &acc = board.get_accum();

    assert(valid_color(stm));

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    const auto acc_stm = (const avx_type *) acc.get_data(stm);
    const auto acc_opp = (const avx_type *) acc.get_data(~stm);

    avx_type res{};
    const auto weights = (const avx_type *) (l1_weights + bucket * L1_SIZE * 2);

    for(int i = 0; i < L1_SIZE / div; i++) {
        res = add_epi32(res, fma(acc_stm[i], weights[i]));
        res = add_epi32(res, fma(acc_opp[i], weights[i + L1_SIZE / div]));
    }

    int32_t output = hor_sum(res) / FT_QUANT + l1_biases[0];
    return (output * EVAL_SCALE) / (FT_QUANT * L1_QUANT);
#else
    int32_t output = 0;

    int16_t *acc_stm = acc.get_data(stm);
    int16_t *acc_opp = acc.get_data(~stm);

    const int offset = bucket * L1_SIZE * 2;
    for(int i = 0; i < L1_SIZE; i++) {
        output += fma(acc_stm[i], l1_weights[offset + i]);
        output += fma(acc_opp[i], l1_weights[offset + L1_SIZE + i]);
    }

    output = output / FT_QUANT + l1_biases[0];
    return (output * EVAL_SCALE) / (FT_QUANT * L1_QUANT);
#endif
}

void NNUE::put(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    avx_type *acc_data = (avx_type *) acc.get_data(view);
    const avx_type *prev_data = acc.is_initialized(view) ? acc_data : (avx_type *) prev.get_data(view);

    const auto weights = (const avx_type *) (ft_weights + idx * L1_SIZE);
    for(int i = 0; i < L1_SIZE / div; i++)
        acc_data[i] = add_epi16(prev_data[i], weights[i]);
#else
    int16_t *acc_view = acc.get_data(view);
    int16_t *prev_view = acc.is_initialized(view) ? acc_view : prev.get_data(view);

    for(int i = 0; i < L1_SIZE; i++)
        acc_view[i] = prev_view[i] + ft_weights[idx * L1_SIZE + i];
#endif

    acc.mark_as_initialized(view);
}

void NNUE::remove(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    avx_type *acc_data = (avx_type *) acc.get_data(view);
    const avx_type *prev_data = acc.is_initialized(view) ? acc_data : (avx_type *) prev.get_data(view);

    const auto weights = (const avx_type *) (ft_weights + idx * L1_SIZE);
    for(int i = 0; i < L1_SIZE / div; i++)
        acc_data[i] = sub_epi16(prev_data[i], weights[i]);
#else
    int16_t *acc_view = acc.get_data(view);
    int16_t *prev_view = acc.is_initialized(view) ? acc_view : prev.get_data(view);

    for(int i = 0; i < L1_SIZE; i++)
        acc_view[i] = prev_view[i] - ft_weights[idx * L1_SIZE + i];
#endif

    acc.mark_as_initialized(view);
}

void NNUE::move(Accum &acc, Accum &prev, Piece pc, Square from, Square to, Square ksq, Color view) const {
    const int from_idx = feature_idx(from, ksq, pc, view);
    const int to_idx = feature_idx(to, ksq, pc, view);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    avx_type *acc_data = (avx_type *) acc.get_data(view);
    const avx_type *prev_data = acc.is_initialized(view) ? acc_data : (avx_type *) prev.get_data(view);

    const auto weights_from = (const avx_type *) (ft_weights + from_idx * L1_SIZE);
    const auto weights_to = (const avx_type *) (ft_weights + to_idx * L1_SIZE);

    for(int i = 0; i < L1_SIZE / div; i++)
        acc_data[i] = add_epi16(prev_data[i], sub_epi16(weights_to[i], weights_from[i]));
#else
    int16_t *acc_view = acc.get_data(view);
    int16_t *prev_view = acc.is_initialized(view) ? acc_view : prev.get_data(view);

    for(int i = 0; i < L1_SIZE; i++) {
        int16_t diff = ft_weights[to_idx * L1_SIZE + i] - ft_weights[from_idx * L1_SIZE + i];
        acc_view[i] = prev_view[i] + diff;
    }
#endif

    acc.mark_as_initialized(view);
}

// global variable
NNUE nnue;

} // namespace NNUE
