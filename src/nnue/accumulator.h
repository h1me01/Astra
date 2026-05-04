#pragma once

#include <array>
#include <cassert>

#include "../chess/types.h"
#include "../ndarray.h"
#include "../search/types.h"
#include "arch.h"
#include "util.h"

namespace astra {
class Board;
}

namespace astra::nnue {

struct DirtyPiece {
    Piece pc = NO_PIECE;
    Square from = NO_SQUARE;
    Square to = NO_SQUARE;

    Piece remove_pc = NO_PIECE;
    Square remove_sq = NO_SQUARE;

    Piece put_pc = NO_PIECE;
    Square put_sq = NO_SQUARE;

    void move(Piece pc, Square from, Square to) {
        assert(valid_piece(pc));
        assert(valid_sq(from));
        assert(valid_sq(to) || piece_type(pc) == PAWN);
        this->pc = pc;
        this->from = from;
        this->to = to;
    }

    void remove(Piece pc, Square sq) {
        assert(valid_piece(pc));
        assert(valid_sq(sq));
        this->remove_pc = pc;
        this->remove_sq = sq;
    }

    void put(Piece pc, Square sq) {
        assert(valid_piece(pc));
        assert(valid_sq(sq));
        this->put_pc = pc;
        this->put_sq = sq;
    }
};

struct Accumulator {
    DirtyPiece dirty_piece;
    NDArray<Square, NUM_COLORS> king_sq;
    NDArray<bool, NUM_COLORS> initialized;
    alignas(64) NDArray<int16_t, NUM_COLORS, FT_SIZE> data;

    Accumulator() { clear(); }

    // doesn't clear data
    void clear() {
        dirty_piece = DirtyPiece();
        initialized.fill(false);
        king_sq.fill(NO_SQUARE);
    }

    void update(const Accumulator& src, Color view);
    void put(const Accumulator& src, Piece pc, Square psq, Square ksq, Color view);
    void remove(const Accumulator& src, Piece pc, Square psq, Square ksq, Color view);
    void move(const Accumulator& src, Piece pc, Square from, Square to, Square ksq, Color view);
};

// idea from koivisto
struct AccumulatorEntry {
    Accumulator accum;
    Bitboard pieces_bb[NUM_COLORS][NUM_PIECE_TYPES];

    void reset();
};

class AccumulatorStack {
    static constexpr int MAX_SIZE = search::MAX_PLY + 1;

  public:
    AccumulatorStack()
        : idx_(0) {}

    void reset(Board& board) {
        idx_ = 0;
        for (Color c : {WHITE, BLACK})
            for (int i = 0; i < 2 * INPUT_BUCKETS; i++)
                entries_[c][i].reset();

        data_(0).clear();

        refresh(WHITE, board);
        refresh(BLACK, board);
    }

    void refresh(Color view, Board& board);

    void push(DirtyPiece dirty_piece, Square w_ksq, Square b_ksq) {
        assert(idx_ < MAX_SIZE - 1);
        assert(valid_piece(dirty_piece.pc));

        idx_++;
        data_(idx_).clear();
        data_(idx_).dirty_piece = dirty_piece;
        data_(idx_).king_sq(WHITE) = w_ksq;
        data_(idx_).king_sq(BLACK) = b_ksq;
    }

    void pop() {
        assert(idx_ > 0);
        idx_--;
    }

    Accumulator& operator[](int i) {
        assert(i >= 0 && i <= idx_);
        return data_(i);
    }

    const Accumulator& operator[](int i) const {
        assert(i >= 0 && i <= idx_);
        return data_(i);
    }

    Accumulator& back() { return data_(idx_); }
    const Accumulator& back() const { return data_(idx_); }
    int size() const { return idx_ + 1; }

  private:
    int idx_;
    NDArray<Accumulator, MAX_SIZE> data_;
    AccumulatorEntry entries_[NUM_COLORS][2 * INPUT_BUCKETS];
};

} // namespace astra::nnue
