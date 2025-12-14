#pragma once

#include <array>
#include <cassert>
#include <memory>

#include "nnue.h"

using namespace chess;

namespace nnue {

struct DirtyPiece {
    Piece pc = NO_PIECE;
    Square from = NO_SQUARE;
    Square to = NO_SQUARE;

    DirtyPiece() {}
    DirtyPiece(Piece pc, Square from, Square to) : pc(pc), from(from), to(to) {}
};

class DirtyPieces {
  public:
    DirtyPieces() : num_dpcs(0) {}

    void reset() {
        num_dpcs = 0;
    }

    void add(Piece pc, Square from, Square to) {
        assert(num_dpcs < 4);
        dpcs[num_dpcs++] = DirtyPiece(pc, from, to);
    }

    DirtyPiece &operator[](int i) {
        assert(i >= 0 && i < num_dpcs);
        return dpcs[i];
    }

    int size() const {
        return num_dpcs;
    }

  private:
    int num_dpcs;
    DirtyPiece dpcs[4]; // only 4 pieces at max can be updated per move
};

class Accum {
  public:
    Accum() {
        reset();
    }

    void update(Accum &prev, Color view);

    void reset() {
        dirty_pcs.reset();
        wksq = bksq = NO_SQUARE;
        initialized[WHITE] = m_needs_refresh[WHITE] = false;
        initialized[BLACK] = m_needs_refresh[BLACK] = false;
    }

    void set_initialized(Color view) {
        this->initialized[view] = true;
    }

    void set_refresh(Color view) {
        this->m_needs_refresh[view] = true;
    }

    void set_info(const DirtyPieces &dpcs, Square wksq, Square bksq) {
        dirty_pcs = dpcs;
        this->wksq = wksq;
        this->bksq = bksq;
    }

    bool is_initialized(Color view) const {
        assert(valid_color(view));
        return initialized[view];
    }

    bool needs_refresh(Color view) const {
        assert(valid_color(view));
        return m_needs_refresh[view];
    }

    int16_t *get_data(Color view) {
        assert(valid_color(view));
        return data[view];
    }

    int16_t *get_data(Color view) const {
        assert(valid_color(view));
        return const_cast<int16_t *>(data[view]);
    }

  private:
    Square wksq, bksq;

    bool initialized[NUM_COLORS];
    bool m_needs_refresh[NUM_COLORS];

    DirtyPieces dirty_pcs;

    alignas(ALIGNMENT) int16_t data[NUM_COLORS][FT_SIZE];
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

    U64 get_piece_bb(Color c, PieceType pt) const {
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
    void refresh(Color view, Board &board, Accum &accum);

  private:
    AccumEntry entries[NUM_COLORS][2 * INPUT_BUCKETS];
};

class AccumList {
  public:
    AccumList() : idx(0), data{} {}

    void decrement() {
        if(idx > 0)
            idx--;
    }

    void increment() {
        assert(idx < MAX_PLY);
        data[++idx].reset();
    }

    void reset(Board &board) {
        idx = 0;
        accum_table->reset();
        accum_table->refresh(WHITE, board, back());
        accum_table->refresh(BLACK, board, back());
    }

    void refresh(Color view, Board &board) {
        accum_table->refresh(view, board, back());
    }

    Accum &operator[](int i) {
        assert(i >= 0 && i <= MAX_PLY);
        return data[i];
    }

    const Accum &operator[](int i) const {
        assert(i >= 0 && i <= MAX_PLY);
        return data[i];
    }

    Accum &back() {
        assert(idx >= 0 && idx <= MAX_PLY);
        return data[idx];
    }

    const Accum &back() const {
        assert(idx >= 0 && idx <= MAX_PLY);
        return data[idx];
    }

    int get_idx() const {
        return idx;
    }

  private:
    int idx;

    std::array<Accum, MAX_PLY + 1> data;
    std::unique_ptr<AccumTable> accum_table = std::make_unique<AccumTable>(AccumTable());
};

} // namespace nnue
