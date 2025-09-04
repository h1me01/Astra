#include <algorithm>
#include <cstring> // std::memcpy

#include "../chess/board.h"
#include "accum.h"
#include "nnue.h"

#include "../third_party/incbin/incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

#if defined(__AVX512F__)

using vec_type = __m512i;

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

using vec_type = __m256i;

#define div (16)
#define madd_epi16 _mm256_madd_epi16
#define add_epi32 _mm256_add_epi32
#define add_epi16 _mm256_add_epi16
#define sub_epi16 _mm256_sub_epi16
#define max_epi16 _mm256_max_epi16
#define min_epi16 _mm256_min_epi16
#define set1_epi16 _mm256_set1_epi16
#define mullo_epi16 _mm256_mullo_epi16

#else

using vec_type = int16_t;

#endif

namespace NNUE {

// helper

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
vec_type fma(const vec_type &input, const vec_type &weight) {
    const vec_type zero{};
    const vec_type ft_clip = set1_epi16(FT_QUANT);

    auto act = min_epi16(max_epi16(input, zero), ft_clip);
    auto product = madd_epi16(mullo_epi16(act, weight), act);

    return product;
}
#endif

int32_t fma(const int16_t &input, const int16_t &weight) {
    int32_t clipped = std::clamp(int32_t(input), 0, FT_QUANT);
    return weight * clipped * clipped;
}

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
int32_t hor_sum(const vec_type &reg) {
#if defined(__AVX512F__)
    return _mm512_reduce_add_epi32(reg);
#else
    const __m128i low = _mm256_castsi256_si128(reg);
    const __m128i high = _mm256_extracti128_si256(reg, 1);
    const __m128i sum128 = _mm_add_epi32(low, high);

    const __m128i sum64 = _mm_add_epi32(sum128, _mm_srli_si128(sum128, 8));
    const __m128i sum32 = _mm_add_epi32(sum64, _mm_srli_si128(sum64, 4));

    return _mm_cvtsi128_si32(sum32);
#endif
}
#endif

int feature_idx(Square psq, Square ksq, Piece pc, Color view) {
    assert(valid_sq(psq));
    assert(valid_sq(ksq));
    assert(valid_piece(pc));
    assert(valid_color(view));

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

    memcpy(ft_weights, &gWeightsData[offset], INPUT_SIZE * FT_SIZE * sizeof(int16_t));
    offset += INPUT_SIZE * FT_SIZE * sizeof(int16_t);

    memcpy(ft_biases, &gWeightsData[offset], FT_SIZE * sizeof(int16_t));
    offset += FT_SIZE * sizeof(int16_t);

    memcpy(l1_weights, &gWeightsData[offset], 2 * FT_SIZE * L1_SIZE * sizeof(int16_t));
    offset += 2 * FT_SIZE * L1_SIZE * sizeof(int16_t);

    memcpy(l1_biases, &gWeightsData[offset], L1_SIZE * sizeof(int16_t));
}

void NNUE::init_accum(Accum &acc) const {
    memcpy(acc.get_data(WHITE), nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
    memcpy(acc.get_data(BLACK), nnue.ft_biases, sizeof(int16_t) * FT_SIZE);
}

int32_t NNUE::forward(Board &board) const {
    board.update_accums();

    const int bucket = (pop_count(board.get_occupancy()) - 2) / 4;
    assert(0 <= bucket && bucket < L1_SIZE);

    const Color stm = board.get_stm();
    const Accum &acc = board.get_accum();

    assert(valid_color(stm));

    int32_t output = 0;

    const auto acc_stm = (const vec_type *) acc.get_data(stm);
    const auto acc_opp = (const vec_type *) acc.get_data(~stm);
    const auto weights = (const vec_type *) (l1_weights + bucket * FT_SIZE * 2);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    vec_type res{};

    for(int i = 0; i < FT_SIZE / div; i++) {
        res = add_epi32(res, fma(acc_stm[i], weights[i]));
        res = add_epi32(res, fma(acc_opp[i], weights[i + FT_SIZE / div]));
    }

    output = hor_sum(res);
#else
    for(int i = 0; i < FT_SIZE; i++) {
        output += fma(acc_stm[i], weights[i]);
        output += fma(acc_opp[i], weights[FT_SIZE + i]);
    }
#endif

    output = output / FT_QUANT + l1_biases[bucket];
    return (output * EVAL_SCALE) / (FT_QUANT * L1_QUANT);
}

void NNUE::put(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);

    auto acc_data = (vec_type *) acc.get_data(view);

    const auto prev_data = acc.is_initialized(view) ? acc_data : (vec_type *) prev.get_data(view);
    const auto weights = (const vec_type *) (ft_weights + idx * FT_SIZE);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    for(int i = 0; i < FT_SIZE / div; i++)
        acc_data[i] = add_epi16(prev_data[i], weights[i]);
#else
    for(int i = 0; i < FT_SIZE; i++)
        acc_data[i] = prev_data[i] + weights[i];
#endif

    acc.mark_as_initialized(view);
}

void NNUE::remove(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);

    auto acc_data = (vec_type *) acc.get_data(view);

    const auto prev_data = acc.is_initialized(view) ? acc_data : (vec_type *) prev.get_data(view);
    const auto weights = (const vec_type *) (ft_weights + idx * FT_SIZE);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    for(int i = 0; i < FT_SIZE / div; i++)
        acc_data[i] = sub_epi16(prev_data[i], weights[i]);
#else
    for(int i = 0; i < FT_SIZE; i++)
        acc_data[i] = prev_data[i] - weights[i];
#endif

    acc.mark_as_initialized(view);
}

void NNUE::move(Accum &acc, Accum &prev, Piece pc, Square from, Square to, Square ksq, Color view) const {
    const int from_idx = feature_idx(from, ksq, pc, view);
    const int to_idx = feature_idx(to, ksq, pc, view);

    auto acc_data = (vec_type *) acc.get_data(view);
    const auto prev_data = acc.is_initialized(view) ? acc_data : (vec_type *) prev.get_data(view);

    const auto weights_from = (const vec_type *) (ft_weights + from_idx * FT_SIZE);
    const auto weights_to = (const vec_type *) (ft_weights + to_idx * FT_SIZE);

#if defined(__AVX512F__) || defined(__AVX2__) || defined(__AVX__)
    for(int i = 0; i < FT_SIZE / div; i++)
        acc_data[i] = add_epi16(prev_data[i], sub_epi16(weights_to[i], weights_from[i]));
#else
    for(int i = 0; i < FT_SIZE; i++)
        acc_data[i] = prev_data[i] + weights_to[i] - weights_from[i];
#endif

    acc.mark_as_initialized(view);
}

// global variable
NNUE nnue;

} // namespace NNUE
