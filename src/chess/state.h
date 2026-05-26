#pragma once

#include <array>
#include <cassert>

#include "../ndarray.h"
#include "bitboard.h"
#include "types.h"
#include "zobrist.h"

namespace astra {

class CastlingRights {
  public:
    CastlingRights()
        : mask_(0) {}

    void add_kingside(Color c) { mask_ |= kingside_castling_bb(c); }
    void add_queenside(Color c) { mask_ |= queenside_castling_bb(c); }

    Hash update(Square from, Square to) {
        Hash hash = zobrist::castling(hash_idx()); // remove old
        mask_ &= ~(sq_bb(from) | sq_bb(to));
        return hash ^ zobrist::castling(hash_idx()); // add new
    }

    bool kingside(const Color c) const {
        assert(is_valid(c));
        return (mask_ & kingside_castling_bb(c)) == kingside_castling_bb(c);
    }

    bool queenside(const Color c) const {
        assert(is_valid(c));
        return (mask_ & queenside_castling_bb(c)) == queenside_castling_bb(c);
    }

    bool any(const Color c) const { return kingside(c) || queenside(c); }
    bool any() const { return any(WHITE) || any(BLACK); }

    bool on_castling_sq(Square sq) const {
        assert(is_valid(sq));
        return mask_ & sq_bb(sq);
    }

    int hash_idx() const { return kingside(WHITE) + 2 * queenside(WHITE) + 4 * kingside(BLACK) + 8 * queenside(BLACK); }

  private:
    Hash mask_;
};

struct StateInfo {
    Piece captured = NO_PIECE;
    Square ep_sq = NO_SQUARE;
    CastlingRights castling_rights;

    int fifty_move_counter = 0;
    int plies_from_null = 0;
    int repetition = 0;

    Hash hash = 0;
    Hash pawn_hash = 0;
    Hash minor_piece_hash = 0;
    NDArray<Hash, NUM_COLORS> non_pawn_hash;

    Bitboard checkers = 0;
    NDArray<Bitboard, NUM_COLORS> pinners;
    NDArray<Bitboard, NUM_COLORS> blockers;
    NDArray<Bitboard, NUM_PIECE_TYPES> check_squares;
};

class StateInfoList {
    static constexpr int MAX_SIZE = 1024;

  public:
    StateInfoList() { clear(); }

    void clear() {
        idx_ = -1;
        data_.fill(StateInfo());
    }

    StateInfo& add() {
        assert(idx_ < MAX_SIZE - 1);
        ++idx_;
        if (idx_ > 0)
            data_(idx_) = data_(idx_ - 1); // copy previous state
        return data_(idx_);
    }

    void pop() {
        assert(!empty());
        --idx_;
    }

    StateInfo& operator()(int i) {
        assert(i >= 0 && i <= idx_);
        return data_(i);
    }

    const StateInfo& operator()(int i) const {
        assert(i >= 0 && i <= idx_);
        return data_(i);
    }

    StateInfo& back() {
        assert(!empty());
        return data_(idx_);
    }

    const StateInfo& back() const {
        assert(!empty());
        return data_(idx_);
    }

    bool empty() const { return idx_ < 0; }
    int size() const { return idx_ + 1; }

  private:
    int idx_;
    NDArray<StateInfo, MAX_SIZE> data_;
};

} // namespace astra
