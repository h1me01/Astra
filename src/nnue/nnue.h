#pragma once

#include <array>
#include <cassert>
#include <cstring>

#include "../chess/misc.h"

using namespace Chess;

namespace Chess {
class Board;
} // namespace Chess

namespace NNUE {

class Accum;

constexpr int BUCKET_SIZE = 12;
constexpr int FEATURE_SIZE = 768;

constexpr int INPUT_SIZE = BUCKET_SIZE * FEATURE_SIZE;
constexpr int FT_SIZE = 1536;
constexpr int L1_SIZE = 1;

constexpr int FT_QUANT = 255;
constexpr int L1_QUANT = 64;

constexpr int EVAL_SCALE = 400;

constexpr int KING_BUCKET[NUM_SQUARES] = {
    0,  1,  2,  3,  3,  2,  1,  0,  //
    4,  5,  6,  7,  7,  6,  5,  4,  //
    8,  8,  9,  9,  9,  9,  8,  8,  //
    10, 10, 10, 10, 10, 10, 10, 10, //
    10, 10, 10, 10, 10, 10, 10, 10, //
    11, 11, 11, 11, 11, 11, 11, 11, //
    11, 11, 11, 11, 11, 11, 11, 11, //
    11, 11, 11, 11, 11, 11, 11, 11, //
};

inline bool needs_refresh(Piece pc, Square from, Square to) {
    if(piece_type(pc) != KING)
        return false;

    Color view = piece_color(pc);
    return KING_BUCKET[rel_sq(view, from)] != KING_BUCKET[rel_sq(view, to)] || sq_file(from) + sq_file(to) == 7;
}

class NNUE {
  public:
    void init();
    void init_accum(Accum &acc) const;

    int32_t forward(Board &board) const;

    void put(        //
        Accum &acc,  //
        Accum &prev, //
        Piece pc,    //
        Square psq,  //
        Square ksq,  //
        Color view   //
    ) const;

    void remove(     //
        Accum &acc,  //
        Accum &prev, //
        Piece pc,    //
        Square psq,  //
        Square ksq,  //
        Color view   //
    ) const;

    void move(       //
        Accum &acc,  //
        Accum &prev, //
        Piece pc,    //
        Square from, //
        Square to,   //
        Square ksq,  //
        Color view   //
    ) const;

  private:
    // (12x768->1536)x2->1
    int16_t ft_weights[INPUT_SIZE * FT_SIZE];
    int16_t ft_biases[FT_SIZE];
    int16_t l1_weights[2 * FT_SIZE * L1_SIZE];
    int16_t l1_biases[L1_SIZE];
};

// global variable
extern NNUE nnue;

} // namespace NNUE
