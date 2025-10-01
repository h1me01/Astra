#pragma once

#include "bitboard.h"
#include "misc.h"

namespace Chess {

class CastlingRights {
  public:
    CastlingRights() : mask(0) {}

    void add_ks(Color c) {
        mask |= OO_MASK[c];
    }

    void add_qs(Color c) {
        mask |= OOO_MASK[c];
    }

    void update(Square from, Square to) {
        mask &= ~(sq_bb(from) | sq_bb(to));
    }

    bool ks(const Color c) const {
        assert(valid_color(c));
        return (mask & OO_MASK[c]) == OO_MASK[c];
    }

    bool qs(const Color c) const {
        assert(valid_color(c));
        return (mask & OOO_MASK[c]) == OOO_MASK[c];
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

    int get_hash_idx() const {
        return ks(WHITE) + 2 * qs(WHITE) + 4 * ks(BLACK) + 8 * qs(BLACK);
    }

  private:
    static constexpr U64 OO_MASK[NUM_COLORS] = {0x90ULL, 0x9000000000000000ULL};
    static constexpr U64 OOO_MASK[NUM_COLORS] = {0x11ULL, 0x1100000000000000ULL};

    U64 mask;
};

} // namespace Chess
