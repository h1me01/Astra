#include <algorithm>
#include <cstring> // std::memcpy

#include "../chess/board.h"
#include "accum.h"
#include "nnue.h"

#include "../third_party/incbin/incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

namespace NNUE {

// activation

#ifdef USE_SIMD
vec_type screlu(const vec_type &input, const vec_type &weight) {
    const vec_type zero{};

    auto act = min_epi16(max_epi16(input, zero), set1_epi16(FT_QUANT));
    auto product = madd_epi16(mullo_epi16(act, weight), act);

    return product;
}
#else
int32_t screlu(const int16_t &input, const int16_t &weight) {
    int32_t clipped = std::clamp(int32_t(input), 0, FT_QUANT);
    return weight * clipped * clipped;
}
#endif

// class NNUE

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

int32_t NNUE::forward(Board &board, const Accum &accum) const {
    assert(accum.is_initialized(WHITE) && accum.is_initialized(BLACK));

    const int bucket = (pop_count(board.get_occupancy()) - 2) / 4;
    assert(0 <= bucket && bucket < L1_SIZE);

    const Color stm = board.get_stm();

    const auto acc_stm = (const vec_type *) accum.get_data(stm);
    const auto acc_opp = (const vec_type *) accum.get_data(~stm);
    const auto weights = (const vec_type *) (l1_weights + bucket * FT_SIZE * 2);

    int32_t output = 0;

#ifdef USE_SIMD
    vec_type res{};
    for(int i = 0; i < FT_SIZE / VEC_DIV; i++) {
        res = add_epi32(res, screlu(acc_stm[i], weights[i]));
        res = add_epi32(res, screlu(acc_opp[i], weights[i + FT_SIZE / VEC_DIV]));
    }

    output = SIMD::hor_sum(res);
#else
    for(int i = 0; i < FT_SIZE; i++) {
        output += screlu(acc_stm[i], weights[i]);
        output += screlu(acc_opp[i], weights[FT_SIZE + i]);
    }
#endif

    output = output / FT_QUANT + l1_biases[bucket];
    return (output * EVAL_SCALE) / (FT_QUANT * L1_QUANT);
}

void NNUE::put(Accum &acc, const Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);
    const auto weights = (const vec_type *) (ft_weights + idx * FT_SIZE);

    auto acc_data = (vec_type *) acc.get_data(view);
    const auto prev_data = acc.is_initialized(view) ? acc_data : (vec_type *) prev.get_data(view);

    for(int i = 0; i < FT_SIZE / VEC_DIV; i++)
        acc_data[i] = VEC_ADD(prev_data[i], weights[i]);

    acc.set_initialized(view);
}

void NNUE::remove(Accum &acc, const Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);
    const auto weights = (const vec_type *) (ft_weights + idx * FT_SIZE);

    auto acc_data = (vec_type *) acc.get_data(view);
    const auto prev_data = acc.is_initialized(view) ? acc_data : (vec_type *) prev.get_data(view);

    for(int i = 0; i < FT_SIZE / VEC_DIV; i++)
        acc_data[i] = VEC_SUB(prev_data[i], weights[i]);

    acc.set_initialized(view);
}

void NNUE::move(Accum &acc, const Accum &prev, Piece pc, Square from, Square to, Square ksq, Color view) const {
    const int from_idx = feature_idx(from, ksq, pc, view);
    const int to_idx = feature_idx(to, ksq, pc, view);
    const auto weights_from = (const vec_type *) (ft_weights + from_idx * FT_SIZE);
    const auto weights_to = (const vec_type *) (ft_weights + to_idx * FT_SIZE);

    auto acc_data = (vec_type *) acc.get_data(view);
    const auto prev_data = acc.is_initialized(view) ? acc_data : (vec_type *) prev.get_data(view);

    for(int i = 0; i < FT_SIZE / VEC_DIV; i++)
        acc_data[i] = VEC_ADD(prev_data[i], VEC_SUB(weights_to[i], weights_from[i]));

    acc.set_initialized(view);
}

// global variable

NNUE nnue;

} // namespace NNUE
