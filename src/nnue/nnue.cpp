#include <algorithm>
#include <cstring> // std::memcpy

#include "../chess/board.h"
#include "accum.h"
#include "nnue.h"

#include "incbin.h"

#define INCBIN_STYLE INCBIN_STYLE_CAMEL

INCBIN(Weights, NNUE_PATH);

namespace NNUE {

// helper

int32_t fma(const int16_t input, const int16_t weight) {
    int32_t clipped = std::clamp(int32_t(input), 0, FT_QUANT);
    return weight * clipped * clipped;
}

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
    Color stm = board.get_stm();
    Accum &acc = board.get_accum();

    assert(valid_color(stm));

    int32_t output = 0;

    int16_t *acc_stm = acc.get_data(stm);
    int16_t *acc_opp = acc.get_data(~stm);

    for(int i = 0; i < FT_SIZE; i++) {
        output += fma(acc_stm[i], l1_weights[i]);
        output += fma(acc_opp[i], l1_weights[FT_SIZE + i]);
    }

    output = output / FT_QUANT + l1_biases[0];
    return (output * EVAL_SCALE) / (FT_QUANT * L1_QUANT);
}

void NNUE::put(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);

    int16_t *acc_view = acc.get_data(view);
    int16_t *prev_view = acc.is_initialized(view) ? acc_view : prev.get_data(view);

    for(int i = 0; i < FT_SIZE; i++)
        acc_view[i] = prev_view[i] + ft_weights[idx * FT_SIZE + i];

    acc.mark_as_initialized(view);
}

void NNUE::remove(Accum &acc, Accum &prev, Piece pc, Square psq, Square ksq, Color view) const {
    const int idx = feature_idx(psq, ksq, pc, view);

    int16_t *acc_view = acc.get_data(view);
    int16_t *prev_view = acc.is_initialized(view) ? acc_view : prev.get_data(view);

    for(int i = 0; i < FT_SIZE; i++)
        acc_view[i] = prev_view[i] - ft_weights[idx * FT_SIZE + i];

    acc.mark_as_initialized(view);
}

void NNUE::move(Accum &acc, Accum &prev, Piece pc, Square from, Square to, Square ksq, Color view) const {
    const int from_idx = feature_idx(from, ksq, pc, view);
    const int to_idx = feature_idx(to, ksq, pc, view);

    int16_t *acc_view = acc.get_data(view);
    int16_t *prev_view = acc.is_initialized(view) ? acc_view : prev.get_data(view);

    for(int i = 0; i < FT_SIZE; i++) {
        int16_t diff = ft_weights[to_idx * FT_SIZE + i] - ft_weights[from_idx * FT_SIZE + i];
        acc_view[i] = prev_view[i] + diff;
    }

    acc.mark_as_initialized(view);
}

// global variable
NNUE nnue;

} // namespace NNUE
