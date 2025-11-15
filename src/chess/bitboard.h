#pragma once

#include "misc.h"

namespace Chess {

namespace Bitboards {
constexpr U64 KS_CASTLE_PATH_MASKk[NUM_COLORS] = {0x60ULL, 0x6000000000000000ULL};
constexpr U64 QS_CASTLE_PATH_MASK[NUM_COLORS] = {0x0EULL, 0x0E00000000000000ULL};

constexpr U64 OO_MASK[NUM_COLORS] = {0x90ULL, 0x9000000000000000ULL};
constexpr U64 OOO_MASK[NUM_COLORS] = {0x11ULL, 0x1100000000000000ULL};

} // namespace Bitboards

constexpr U64 file_mask(File file) {
    assert(file >= 0 && file < 8);
    return 0x0101010101010101ULL << file;
}

constexpr U64 rank_mask(Rank rank) {
    assert(rank >= 0 && rank < 8);
    return 0xFFULL << (rank * 8);
}

constexpr U64 ks_castle_path_mask(Color c) {
    assert(valid_color(c));
    return Bitboards::KS_CASTLE_PATH_MASKk[c];
}

constexpr U64 qs_castle_path_mask(Color c) {
    assert(valid_color(c));
    return Bitboards::QS_CASTLE_PATH_MASK[c];
}

constexpr U64 oo_mask(Color c) {
    assert(valid_color(c));
    return Bitboards::OO_MASK[c];
}

constexpr U64 ooo_mask(Color c) {
    assert(valid_color(c));
    return Bitboards::OOO_MASK[c];
}

inline U64 sq_bb(Square sq) {
    return (1ULL << sq);
}

// returns index of least significant bit in bitboard
inline Square lsb(const U64 b) {
    return Square(__builtin_ctzll(b));
}

// returns number of bits in bitboard
inline int pop_count(U64 b) {
    return __builtin_popcountll(b);
}

// returns and removes index of the least significant bit in bitboard
inline Square pop_lsb(U64 &b) {
    int n = lsb(b);
    b &= b - 1;
    return Square(n);
}

template <Direction d> //
constexpr U64 shift(const U64 b) {
    switch(d) {
        // clang-format off
        case NORTH:      return b << 8;
        case SOUTH:      return b >> 8;
        case EAST:       return (b & ~file_mask(FILE_H)) << 1;
        case WEST:       return (b & ~file_mask(FILE_A)) >> 1;
        case NORTH_EAST: return (b & ~file_mask(FILE_H)) << 9;
        case NORTH_WEST: return (b & ~file_mask(FILE_A)) << 7;
        case SOUTH_EAST: return (b & ~file_mask(FILE_H)) >> 7;
        case SOUTH_WEST: return (b & ~file_mask(FILE_A)) >> 9;
        default:         return 0;
        // clang-format on
    }
}

U64 between_bb(Square sq1, Square sq2);
U64 line(Square sq1, Square sq2);

U64 get_pawn_attacks(Color c, Square sq);

U64 get_attacks(PieceType pt, Square sq, const U64 occ = 0);

template <PieceType pt> //
U64 get_attacks(Square sq, const U64 occ = 0) {
    return get_attacks(pt, sq, occ);
}

namespace Bitboards {

void init();

} // namespace Bitboards

} // namespace Chess
