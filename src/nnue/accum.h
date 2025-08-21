#pragma once

#include <array>
#include <cassert>

#include "nnue.h"

using namespace Chess;

namespace NNUE {

class Accum {
  public:
    void reset() {
        initialized[WHITE] = false;
        initialized[BLACK] = false;
    }

    void mark_as_initialized(Color view) {
        this->initialized[view] = true;
    }

    bool is_initialized(Color view) const {
        return initialized[view];
    }

    int16_t *get_data(Color view) {
        return data[view];
    }

  private:
    bool initialized[NUM_COLORS] = {false, false};
    int16_t data[NUM_COLORS][FT_SIZE];
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
    Accum acc;
    U64 piece_bb[NUM_COLORS][NUM_PIECE_TYPES];
};

class AccumTable {
  public:
    void reset();
    void refresh(Board &board, Color view);

  private:
    AccumEntry entries[NUM_COLORS][2 * BUCKET_SIZE];
};

} // namespace NNUE
