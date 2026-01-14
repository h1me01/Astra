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

    DirtyPiece(Piece pc, Square from, Square to)
        : pc(pc),
          from(from),
          to(to) {}
};

class DirtyPieces {
  public:
    DirtyPieces() {
        reset();
    }

    void reset() {
        idx = 0;
        needs_refresh[WHITE] = needs_refresh[BLACK] = false;
    }

    void add(Piece pc, Square from, Square to) {
        assert(valid_piece(pc) && idx < 3);
        dpcs[idx++] = DirtyPiece(pc, from, to);

        if (piece_type(pc) == KING) {
            Color c = piece_color(pc);
            needs_refresh[c] |= INPUT_BUCKET[rel_sq(c, from)] != INPUT_BUCKET[rel_sq(c, to)] //
                                || sq_file(from) + sq_file(to) == 7;
        }
    }

    void pop() {
        assert(idx > 0);
        if (idx > 0)
            idx--;
    }

    DirtyPiece& operator[](int i) {
        assert(i >= 0 && i < idx);
        return dpcs[i];
    }

    int size() const {
        return idx;
    }

  public:
    Square wksq, bksq;
    bool needs_refresh[NUM_COLORS];

  private:
    int idx;
    DirtyPiece dpcs[3]; // only 3 pieces at max can be updated per move
};

struct Accum {
    bool initialized[NUM_COLORS];
    DirtyPieces dpcs;
    alignas(ALIGNMENT) int16_t data[NUM_COLORS][FT_SIZE];

    Accum() {
        reset();
    }

    void reset() {
        dpcs.reset();
        initialized[WHITE] = initialized[BLACK] = false;
    }

    bool needs_refresh(Color c) const {
        assert(valid_color(c));
        return dpcs.needs_refresh[c];
    }

    void update(Accum& prev, Color view);
};

// idea from koivisto
struct AccumEntry {
    Accum accum;
    U64 pieces_bb[NUM_COLORS][NUM_PIECE_TYPES];

    void reset() {
        std::memset(pieces_bb, 0, sizeof(pieces_bb));
        nnue.init_accum(accum);
    }

    U64& operator()(Color c, PieceType pt) {
        return pieces_bb[c][pt];
    }

    const U64& operator()(Color c, PieceType pt) const {
        return pieces_bb[c][pt];
    }
};

class AccumList {
  public:
    AccumList()
        : idx(0) {}

    void reset(Board& board) {
        idx = 0;

        for (Color c : {WHITE, BLACK})
            for (int i = 0; i < 2 * INPUT_BUCKETS; i++)
                entries[c][i].reset();

        refresh(WHITE, board);
        refresh(BLACK, board);
    }

    void refresh(Color view, Board& board);

    void increment(const DirtyPieces& dirty_pieces) {
        idx++;
        assert(idx < MAX_PLY);
        data[idx].reset();
        data[idx].dpcs = dirty_pieces;
    }

    void decrement() {
        assert(idx > 0);
        if (idx > 0)
            idx--;
    }

    Accum& operator[](int i) {
        assert(i >= 0 && i <= MAX_PLY);
        return data[i];
    }

    const Accum& operator[](int i) const {
        assert(i >= 0 && i <= MAX_PLY);
        return data[i];
    }

    Accum& back() {
        assert(idx >= 0 && idx <= MAX_PLY);
        return data[idx];
    }

    const Accum& back() const {
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
