#pragma once

#include "bitboard.h"
#include "misc.h"

namespace chess {

class CastlingRights {
  public:
    CastlingRights() : mask(0) {}

    void add_ks(Color c) {
        mask |= oo_mask(c);
    }

    void add_qs(Color c) {
        mask |= ooo_mask(c);
    }

    void update(Square from, Square to) {
        mask &= ~(sq_bb(from) | sq_bb(to));
    }

    bool ks(const Color c) const {
        assert(valid_color(c));
        return (mask & oo_mask(c)) == oo_mask(c);
    }

    bool qs(const Color c) const {
        assert(valid_color(c));
        return (mask & ooo_mask(c)) == ooo_mask(c);
    }

    bool any(const Color c) const {
        return ks(c) || qs(c);
    }

    bool any() const {
        return any(WHITE) || any(BLACK);
    }

    bool on_castle_sq(Square sq) const {
        assert(valid_sq(sq));
        return mask & sq_bb(sq);
    }

    int hash_idx() const {
        return ks(WHITE) + 2 * qs(WHITE) + 4 * ks(BLACK) + 8 * qs(BLACK);
    }

  private:
    U64 mask;
};

} // namespace chess
