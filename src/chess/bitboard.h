#pragma once

#include <cassert>
#include <cstdlib>

#if defined(__BMI2__)
#include <immintrin.h>
#endif

#include "types.h"

namespace astra {

constexpr Bitboard sq_bb(Square sq) {
    assert(valid_sq(sq));
    return (1ULL << sq);
}

template <File f>
constexpr Bitboard file_bb() {
    assert(valid_file(f));
    return 0x0101010101010101ULL << f;
}

constexpr Bitboard file_bb(File f) {
    assert(valid_file(f));
    return 0x0101010101010101ULL << f;
}

template <Rank r>
constexpr Bitboard rank_bb() {
    assert(valid_rank(r));
    return 0xFFULL << (r * 8);
}

constexpr Bitboard rank_bb(Rank r) {
    assert(valid_rank(r));
    return 0xFFULL << (r * 8);
}

constexpr Bitboard kingside_castling_bb(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x90ULL : 0x9000000000000000ULL;
}

constexpr Bitboard queenside_castling_bb(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x11ULL : 0x1100000000000000ULL;
}

constexpr Bitboard kingside_castling_path_bb(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x60ULL : 0x6000000000000000ULL;
}

constexpr Bitboard queenside_castling_path_bb(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x0EULL : 0x0E00000000000000ULL;
}

inline Square lsb(const Bitboard b) { return static_cast<Square>(__builtin_ctzll(b)); }

inline int pop_count(Bitboard b) { return __builtin_popcountll(b); }

inline Square pop_lsb(Bitboard& b) {
    int n = lsb(b);
    b &= b - 1;
    return static_cast<Square>(n);
}

template <Direction d>
constexpr Bitboard shift(const Bitboard b) {
    switch (d) {
    case NORTH:
        return b << 8;
    case SOUTH:
        return b >> 8;
    case EAST:
        return (b & ~file_bb<FILE_H>()) << 1;
    case WEST:
        return (b & ~file_bb<FILE_A>()) >> 1;
    case NORTH_EAST:
        return (b & ~file_bb<FILE_H>()) << 9;
    case NORTH_WEST:
        return (b & ~file_bb<FILE_A>()) << 7;
    case SOUTH_EAST:
        return (b & ~file_bb<FILE_H>()) >> 7;
    case SOUTH_WEST:
        return (b & ~file_bb<FILE_A>()) >> 9;
    default:
        return 0;
    }
}

constexpr Bitboard valid_destination(Square sq, int step) {
    Square to = static_cast<Square>(static_cast<int>(sq) + step);
    return valid_sq(to) && std::abs(sq_file(sq) - sq_file(to)) <= 2 ? sq_bb(to) : 0;
};

namespace bitboards {
void init();
} // namespace bitboards

Bitboard between_bb(Square sq1, Square sq2);
Bitboard line(Square sq1, Square sq2);

template <Color c>
constexpr Bitboard pawn_attacks_bb(Square sq) {
    assert(valid_sq(sq));
    assert(valid_color(c));

    Bitboard b = sq_bb(sq);
    return (c == WHITE) ? shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b) //
                        : shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
}

constexpr Bitboard pawn_attacks_bb(Color c, Square sq) {
    return (c == WHITE) ? pawn_attacks_bb<WHITE>(sq) : pawn_attacks_bb<BLACK>(sq);
}

constexpr Bitboard knight_attacks_bb(Square sq) {
    Bitboard b = 0;
    for (int step : {-17, -15, -10, -6, 6, 10, 15, 17})
        b |= valid_destination(sq, step);
    return b;
}

constexpr Bitboard king_attacks_bb(Square sq) {
    Bitboard b = 0;
    for (int step : {-9, -8, -7, -1, 1, 7, 8, 9})
        b |= valid_destination(sq, step);
    return b;
}

Bitboard rook_attacks_bb(Square sq, const Bitboard occ);
Bitboard bishop_attacks_bb(Square sq, const Bitboard occ);

template <PieceType pt>
Bitboard attacks_bb(Square sq);
Bitboard attacks_bb(PieceType pt, Square sq);

template <PieceType pt>
Bitboard attacks_bb(Square sq, const Bitboard occ) {
    assert(pt != PAWN);
    assert(valid_sq(sq));
    assert(valid_piece_type(pt));

    switch (pt) {
    case ROOK:
        return rook_attacks_bb(sq, occ);
    case BISHOP:
        return bishop_attacks_bb(sq, occ);
    case QUEEN:
        return rook_attacks_bb(sq, occ) | bishop_attacks_bb(sq, occ);
    default:
        return attacks_bb<pt>(sq);
    }
}

inline Bitboard attacks_bb(PieceType pt, Square sq, const Bitboard occ) {
    assert(pt != PAWN);
    assert(valid_sq(sq));
    assert(valid_piece_type(pt));

    switch (pt) {
    case ROOK:
        return attacks_bb<ROOK>(sq, occ);
    case BISHOP:
        return attacks_bb<BISHOP>(sq, occ);
    case QUEEN:
        return attacks_bb<QUEEN>(sq, occ);
    default:
        return attacks_bb(pt, sq);
    }
}

} // namespace astra
