#pragma once

#include "types.h"

namespace Chess {

constexpr U64 MASK_FILE[8] = {
    0x0101010101010101ULL, //
    0x0202020202020202ULL, //
    0x0404040404040404ULL, //
    0x0808080808080808ULL, //
    0x1010101010101010ULL, //
    0x2020202020202020ULL, //
    0x4040404040404040ULL, //
    0x8080808080808080ULL  //
};

constexpr U64 MASK_RANK[8] = {
    0x00000000000000FFULL, //
    0x000000000000FF00ULL, //
    0x0000000000FF0000ULL, //
    0x00000000FF000000ULL, //
    0x000000FF00000000ULL, //
    0x0000FF0000000000ULL, //
    0x00FF000000000000ULL, //
    0xFF00000000000000ULL  //
};

constexpr U64 MASK_DIAGONAL[15] = {
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

constexpr U64 MASK_ANTI_DIAGONAL[15] = {
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
    // clang-format off
    return d == NORTH      ? b << 8 
         : d == SOUTH      ? b >> 8 
         : d == EAST       ? (b & ~MASK_FILE[FILE_H]) << 1 
         : d == WEST       ? (b & ~MASK_FILE[FILE_A]) >> 1 
         : d == NORTH_EAST ? (b & ~MASK_FILE[FILE_H]) << 9 
         : d == NORTH_WEST ? (b & ~MASK_FILE[FILE_A]) << 7 
         : d == SOUTH_EAST ? (b & ~MASK_FILE[FILE_H]) >> 7 
         : d == SOUTH_WEST ? (b & ~MASK_FILE[FILE_A]) >> 9 
         : 0;
    // clang-format on
}

void init_lookup_tables();

U64 between_bb(Square sq1, Square sq2);
U64 line(Square sq1, Square sq2);

U64 get_pawn_attacks(Color c, Square sq);

U64 get_attacks(PieceType pt, Square sq, const U64 occ = 0);

} // namespace Chess
