#pragma once

#include "types.h"

namespace Chess {

constexpr U64 file_mask(File file) {
    assert(file >= 0 && file < 8);
    return 0x0101010101010101ULL << file;
}

constexpr U64 rank_mask(Rank rank) {
    assert(rank >= 0 && rank < 8);
    return 0xFFULL << (rank * 8);
}

constexpr U64 diagonal_mask(int idx) {
    constexpr U64 MASK[15] = {
        0x0000000000000080ULL, //
        0x0000000000008040ULL, //
        0x0000000000804020ULL, //
        0x0000000080402010ULL, //
        0x0000008040201008ULL, //
        0x0000804020100804ULL, //
        0x0080402010080402ULL, //
        0x8040201008040201ULL, //
        0x4020100804020100ULL, //
        0x2010080402010000ULL, //
        0x1008040201000000ULL, //
        0x0804020100000000ULL, //
        0x0402010000000000ULL, //
        0x0201000000000000ULL, //
        0x0100000000000000ULL  //
    };

    assert(idx >= 0 && idx < 15);
    return MASK[idx];
}

constexpr U64 anti_diag_mask(int idx) {
    constexpr U64 MASK[15] = {
        0x0000000000000001ULL, //
        0x0000000000000102ULL, //
        0x0000000000010204ULL, //
        0x0000000001020408ULL, //
        0x0000000102040810ULL, //
        0x0000010204081020ULL, //
        0x0001020408102040ULL, //
        0x0102040810204080ULL, //
        0x0204081020408000ULL, //
        0x0408102040800000ULL, //
        0x0810204080000000ULL, //
        0x1020408000000000ULL, //
        0x2040800000000000ULL, //
        0x4080000000000000ULL, //
        0x8000000000000000ULL  //
    };

    assert(idx >= 0 && idx < 15);
    return MASK[idx];
}

constexpr U64 OO_BLOCKERS_MASK[NUM_COLORS] = {0x60ULL, 0x6000000000000000ULL};
constexpr U64 OOO_BLOCKERS_MASK[NUM_COLORS] = {0xeULL, 0xE00000000000000ULL};

inline U64 square_bb(Square sq) {
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

void init_lookup_tables();

U64 between_bb(Square sq1, Square sq2);
U64 line(Square sq1, Square sq2);

U64 get_pawn_attacks(Color c, Square sq);

U64 get_attacks(PieceType pt, Square sq, const U64 occ = 0);

} // namespace Chess
