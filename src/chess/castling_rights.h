#pragma once

#include "misc.h"
#include "bitboard.h"

namespace Chess {

class CastlingRights {
  public:
    CastlingRights() : mask(0) {}

    void add_kingside(Color c) {
        mask |= OO_MASK[c];
    }

    void add_queenside(Color c) {
        mask |= OOO_MASK[c];
    }

    void update(U64 from, U64 to) {
        mask &= ~(from | to);
    }

    bool kingside(const Color c) const {
        assert(valid_color(c));
        return (mask & OO_MASK[c]) == OO_MASK[c];
    }

    bool queenside(const Color c) const {
        assert(valid_color(c));
        return (mask & OOO_MASK[c]) == OOO_MASK[c];
    }

    bool any(const Color c) const {
        return kingside(c) || queenside(c);
    }

    bool any() const {
        return any(WHITE) || any(BLACK);
    }

    bool on_castle_sq(Square sq) const {
        assert(valid_sq(sq));
        return mask & square_bb(sq);
    }

    int get_hash_idx() const {
        return kingside(WHITE) + 2 * queenside(WHITE) + 4 * kingside(BLACK) + 8 * queenside(BLACK);
    }

  private:
    static constexpr U64 OO_MASK[NUM_COLORS] = {0x90ULL, 0x9000000000000000ULL};
    static constexpr U64 OOO_MASK[NUM_COLORS] = {0x11ULL, 0x1100000000000000ULL};

    U64 mask;
};

} // namespace Chess
