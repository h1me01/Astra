#pragma once

#include <array>
#include <cassert>
#include <cstring>

#include "../chess/misc.h"
#include "constants.h"

#include "simd.h"

using namespace chess;

namespace chess {
class Board;
} // namespace chess

namespace nnue {

class Accum;

// (13x768->1536)x2->(16->32->1)x8->selected output

inline bool needs_refresh(Piece pc, Move m) {
    assert(m);

    if (piece_type(pc) != KING)
        return false;

    Color view = piece_color(pc);
    return INPUT_BUCKET[rel_sq(view, m.from())] != INPUT_BUCKET[rel_sq(view, m.to())] ||
           sq_file(m.from()) + sq_file(m.to()) == 7;
}

template <typename T, size_t N>
class LayerOutput {
  public:
    LayerOutput() {
        std::memset(data, 0, sizeof(T) * N);
    }

    LayerOutput(T* init_data) {
        std::memcpy(data, init_data, sizeof(T) * N);
    }

    T operator[](size_t idx) const {
        assert(idx < N);
        return data[idx];
    }

    T& operator[](size_t idx) {
        assert(idx < N);
        return data[idx];
    }

    operator T*() {
        return data;
    }

    operator const T*() const {
        return data;
    }

  private:
    alignas(ALIGNMENT) T data[N];
};

class NNUE {
  public:
    void init();
    void init_accum(Accum& acc) const;

    int32_t forward(Board& board, const Accum& acc);

    void put(Accum& acc, const Accum& prev, Piece pc, Square psq, Square ksq, Color view) const;
    void remove(Accum& acc, const Accum& prev, Piece pc, Square psq, Square ksq, Color view) const;
    void move(Accum& acc, const Accum& prev, Piece pc, Square from, Square to, Square ksq, Color view) const;

  private:
    using NNZOutput = std::pair<int, LayerOutput<uint16_t, FT_SIZE / 4>>;

  private:
    alignas(ALIGNMENT) int16_t ft_weights[INPUT_SIZE * FT_SIZE];
    alignas(ALIGNMENT) int16_t ft_biases[FT_SIZE];
    alignas(ALIGNMENT) int8_t l1_weights[OUTPUT_BUCKETS][FT_SIZE * L1_SIZE];
    alignas(ALIGNMENT) float l1_biases[OUTPUT_BUCKETS][L1_SIZE];
    alignas(ALIGNMENT) float l2_weights[OUTPUT_BUCKETS][L1_SIZE * L2_SIZE];
    alignas(ALIGNMENT) float l2_biases[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l3_weights[OUTPUT_BUCKETS][L2_SIZE];
    alignas(ALIGNMENT) float l3_biases[OUTPUT_BUCKETS];

    alignas(ALIGNMENT) uint16_t nnz_lookup[256][8];

    int feature_idx(Piece pc, Square psq, Square ksq, Color view) const {
        assert(valid_piece(pc) && valid_sq(psq));

        if (sq_file(ksq) > FILE_D)
            psq = Square(psq ^ 7); // mirror psq horizontally if king is on other half

        return rel_sq(view, psq)                 //
               + piece_type(pc) * 64             //
               + (piece_color(pc) != view) * 384 //
               + INPUT_BUCKET[rel_sq(view, ksq)] * FEATURE_SIZE;
    }

    LayerOutput<uint8_t, FT_SIZE> prep_l1_input(const Color stm, const Accum& acc);
    NNZOutput find_nnz(const LayerOutput<uint8_t, FT_SIZE>& input);
    LayerOutput<float, L1_SIZE> forward_l1(int bucket, const LayerOutput<uint8_t, FT_SIZE>& input);
    LayerOutput<float, L2_SIZE> forward_l2(int bucket, const LayerOutput<float, L1_SIZE>& input);
    float forward_l3(int bucket, const LayerOutput<float, L2_SIZE>& input);
};

// Global Variable

extern NNUE nnue;

} // namespace nnue
