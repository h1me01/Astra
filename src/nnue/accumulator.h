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
} // namespace astra

namespace astra::nnue {

struct DirtyPiece {
    Piece pc = NO_PIECE;
    Square from = NO_SQUARE;
    Square to = NO_SQUARE;

    constexpr DirtyPiece() = default;
    constexpr DirtyPiece(Piece pc, Square from, Square to)
        : pc(pc),
          from(from),
          to(to) {}

    static constexpr DirtyPiece added(Piece pc, Square to) { return DirtyPiece(pc, NO_SQUARE, to); }
    static constexpr DirtyPiece removed(Piece pc, Square from) { return DirtyPiece(pc, from, NO_SQUARE); }
    static constexpr DirtyPiece moved(Piece pc, Square from, Square to) { return DirtyPiece(pc, from, to); }

    constexpr bool is_add() const { return is_valid(to) && !is_valid(from); }
    constexpr bool is_remove() const { return is_valid(from) && !is_valid(to); }
    constexpr bool is_move() const { return is_valid(from) && is_valid(to); }
};

class DirtyPieceList {
    static constexpr int MAX_SIZE = 3;

  public:
    DirtyPieceList() { clear(); }

    void clear() {
        idx_ = -1;
        data_.fill(DirtyPiece());
    }

    void add(DirtyPiece dp) {
        assert(idx_ < MAX_SIZE - 1);
        assert(is_valid(dp.pc));
        assert(is_valid(dp.from) || is_valid(dp.to));
        ++idx_;
        data_(idx_) = dp;
    }

    void pop() {
        assert(!empty());
        --idx_;
    }

    DirtyPiece& operator()(int i) {
        assert(i >= 0 && i <= idx_);
        return data_(i);
    }

    const DirtyPiece& operator()(int i) const {
        assert(i >= 0 && i <= idx_);
        return data_(i);
    }

    DirtyPiece* begin() { return size() ? &data_(0) : nullptr; }
    DirtyPiece* end() { return size() ? (&data_(0) + size()) : nullptr; }
    const DirtyPiece* begin() const { return size() ? &data_(0) : nullptr; }
    const DirtyPiece* end() const { return size() ? (&data_(0) + size()) : nullptr; }

    bool empty() const { return idx_ < 0; }
    int size() const { return idx_ + 1; }

  private:
    int idx_;
    NDArray<DirtyPiece, MAX_SIZE> data_;
};

struct Accumulator {
    DirtyPieceList dirty_pieces;
    NDArray<Square, NUM_COLORS> king_sq;
    NDArray<bool, NUM_COLORS> initialized;
    alignas(64) NDArray<int16_t, NUM_COLORS, FT_SIZE> data;

    Accumulator() { clear(); }

    // doesn't clear data
    void clear() {
        dirty_pieces.clear();
        initialized.fill(false);
        king_sq.fill(NO_SQUARE);
    }

    bool should_refresh(Color view) const {
        assert(is_valid(view));

        auto it = std::ranges::find_if(dirty_pieces, [](const auto& dp) { return dp.is_move(); });
        if (it == dirty_pieces.end())
            return false;

        if (type_of(it->pc) != KING || color_of(it->pc) != view)
            return false;

        return INPUT_BUCKET(relative_sq(view, it->from)) != INPUT_BUCKET(relative_sq(view, it->to)) ||
               (file_of(it->from) > FILE_D) != (file_of(it->to) > FILE_D);
    }

    void update(const Accumulator& src, Color view);
    void put(const Accumulator& src, Piece pc, Square psq, Square ksq, Color view);
    void remove(const Accumulator& src, Piece pc, Square psq, Square ksq, Color view);
    void move(const Accumulator& src, Piece pc, Square from, Square to, Square ksq, Color view);
};

// idea from koivisto
struct AccumulatorEntry {
    Accumulator accum;
    NDArray<Bitboard, NUM_COLORS, NUM_PIECE_TYPES> pieces_bb;

    void reset();
};

class AccumulatorList {
    static constexpr int MAX_SIZE = search::MAX_PLY + 1;

  public:
    AccumulatorList()
        : idx_(0) {}

    void reset(Board& board) {
        idx_ = 0;
        for (Color c : {WHITE, BLACK})
            for (int i = 0; i < 2 * INPUT_BUCKETS; ++i)
                entries_(c, i).reset();

        data_(0).clear();

        refresh(WHITE, board);
        refresh(BLACK, board);
    }

    void refresh(Color view, Board& board);

    void add(DirtyPieceList dirty_pieces, Square w_ksq, Square b_ksq) {
        assert(idx_ < MAX_SIZE - 1);

        ++idx_;
        data_(idx_).clear();
        data_(idx_).dirty_pieces = dirty_pieces;
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
    NDArray<AccumulatorEntry, NUM_COLORS, 2 * INPUT_BUCKETS> entries_;
};

} // namespace astra::nnue
