#pragma once

#include <array>
#include <cassert>

#include "nnue.h"

using namespace Chess;

namespace NNUE {

struct DirtyPiece {
    Piece pc = NO_PIECE;
    Square from = NO_SQUARE;
    Square to = NO_SQUARE;

    DirtyPiece() {}
    DirtyPiece(Piece pc, Square from, Square to) : pc(pc), from(from), to(to) {}
};

class Accum {
  public:
    void reset();

    void put_piece(Piece pc, Square to, Square wksq, Square bksq);
    void remove_piece(Piece pc, Square from, Square wksq, Square bksq);
    void move_piece(Piece pc, Square from, Square to, Square wksq, Square bksq);

    void update(Accum &prev, Color view);

    void mark_as_initialized(Color view) {
        this->initialized[view] = true;
    }

    void set_refresh(Color view) {
        this->m_needs_refresh[view] = true;
    }

    bool is_initialized(Color view) const {
        return initialized[view];
    }

    bool needs_refresh(Color view) const {
        return m_needs_refresh[view];
    }

    int16_t *get_data(Color view) {
        return data[view];
    }

  private:
    Square wksq, bksq;

    bool initialized[NUM_COLORS] = {false, false};
    bool m_needs_refresh[NUM_COLORS] = {false, false};

    alignas(ALIGNMENT) int16_t data[NUM_COLORS][FT_SIZE];

    int num_dpcs = 0;
    // an accumulator can update at max only 4 pieces per move:
    // such case might be pawn captures piece on promotion rank:
    //  1. remove captured piece
    //  2. move pawn to target square
    //  3. add promotion piece to target square
    //  4. remove pawn from target square
    DirtyPiece dpcs[4];
};

// idea from koivisto
class AccumEntry {
  public:
    void reset() {
        memset(piece_bb, 0, sizeof(piece_bb));
        nnue.init_accum(acc);
    }

    void set_piecebb(Color c, PieceType pt, U64 bb) {
        piece_bb[c][pt] = bb;
    }

    U64 get_piecebb(Color c, PieceType pt) const {
        return piece_bb[c][pt];
    }

    Accum &get_accum() {
        return acc;
    }

  private:
    U64 piece_bb[NUM_COLORS][NUM_PIECE_TYPES];
    Accum acc;
};

class AccumTable {
  public:
    void reset();
    void refresh(Board &board, Color view);

  private:
    AccumEntry entries[NUM_COLORS][2 * BUCKET_SIZE];
};

} // namespace NNUE
