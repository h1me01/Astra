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
    DirtyPieces() : idx(0) {}

    void reset() {
        idx = 0;
    }

    void pop() {
        assert(idx > 0);
        idx--;
    }

    void add(Piece pc, Square from, Square to) {
        assert(idx < 3);
        dpcs[idx++] = DirtyPiece(pc, from, to);
    }

    DirtyPiece &operator[](int i) {
        assert(i >= 0 && i < idx);
        return dpcs[i];
    }

    int size() const {
        return idx;
    }

  private:
    int idx;
    DirtyPiece dpcs[3]; // only 3 pieces at max can be updated per move
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
        initialized[WHITE] = refresh[WHITE] = false;
        initialized[BLACK] = refresh[BLACK] = false;
    }

    void set_initialized(Color view) {
        assert(valid_color(view));
        this->initialized[view] = true;
    }

    void set_refresh(Color view) {
        assert(valid_color(view));
        this->refresh[view] = true;
    }

    void update(const DirtyPieces &dpcs, Square wksq, Square bksq) {
        assert(valid_sq(wksq) && valid_sq(bksq));
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
        return refresh[view];
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
    bool refresh[NUM_COLORS];

    DirtyPieces dirty_pcs;

    alignas(ALIGNMENT) int16_t data[NUM_COLORS][FT_SIZE];
};

// idea from koivisto
class AccumEntry {
  public:
    void reset() {
        std::memset(pieces_bb, 0, sizeof(pieces_bb));
        nnue.init_accum(acc);
    }

    U64 &operator()(Color c, PieceType pt) {
        return pieces_bb[c][pt];
    }

    const U64 &operator()(Color c, PieceType pt) const {
        return pieces_bb[c][pt];
    }

    Accum &get_accum() {
        return acc;
    }

  private:
    Accum acc;
    U64 pieces_bb[NUM_COLORS][NUM_PIECE_TYPES];
};

class AccumList {
  public:
    AccumList() : idx(0), data{} {}

    void decrement() {
        if(idx > 0)
            idx--;
    }

    Accum &increment() {
        idx++;
        assert(idx < MAX_PLY);
        data[idx].reset();
        return data[idx];
    }

    void reset(Board &board) {
        idx = 0;

        for(Color c : {WHITE, BLACK})
            for(int i = 0; i < 2 * INPUT_BUCKETS; i++)
                entries[c][i].reset();

        refresh(WHITE, board);
        refresh(BLACK, board);
    }

    void refresh(Color view, Board &board);

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

    int size() const {
        return idx + 1;
    }

  private:
    int idx;

    std::array<Accum, MAX_PLY + 1> data;
    AccumEntry entries[NUM_COLORS][2 * INPUT_BUCKETS];
};

} // namespace nnue
