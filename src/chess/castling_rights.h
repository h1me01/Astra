#pragma once

#include "bitboard.h"
#include "misc.h"

namespace chess {

class CastlingRights {
  public:
    CastlingRights() : mask(0) {}

    void add_kingside(Color c) {
        mask |= oo_mask(c);
    }

    void add_queenside(Color c) {
        mask |= ooo_mask(c);
    }

    void update(Square from, Square to) {
        mask &= ~(sq_bb(from) | sq_bb(to));
    }

    bool kingside(const Color c) const {
        assert(valid_color(c));
        return (mask & oo_mask(c)) == oo_mask(c);
    }

    bool queenside(const Color c) const {
        assert(valid_color(c));
        return (mask & ooo_mask(c)) == ooo_mask(c);
    }

    bool any(const Color c) const {
        return kingside(c) || queenside(c);
    }

    bool any() const {
        return any(WHITE) || any(BLACK);
    }

    bool on_castling_sq(Square sq) const {
        assert(valid_sq(sq));
        return mask & sq_bb(sq);
    }

    int hash_idx() const {
        return kingside(WHITE) + 2 * queenside(WHITE) + 4 * kingside(BLACK) + 8 * queenside(BLACK);
    }

  private:
    U64 mask;
};

} // namespace chess
