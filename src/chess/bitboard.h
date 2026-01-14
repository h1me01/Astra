#pragma once

#if defined(__BMI2__)
#include <immintrin.h>
#endif

#include "misc.h"

namespace chess {

constexpr U64 sq_bb(Square sq) {
    return (1ULL << sq);
}

template <File f>
constexpr U64 file_mask() {
    assert(valid_file(f));
    return 0x0101010101010101ULL << f;
}

constexpr U64 file_mask(File f) {
    assert(valid_file(f));
    return 0x0101010101010101ULL << f;
}

template <Rank r>
constexpr U64 rank_mask() {
    assert(valid_rank(r));
    return 0xFFULL << (r * 8);
}

constexpr U64 rank_mask(Rank r) {
    assert(valid_rank(r));
    return 0xFFULL << (r * 8);
}

constexpr U64 ks_castle_path_mask(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x60ULL : 0x6000000000000000ULL;
}

constexpr U64 qs_castle_path_mask(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x0EULL : 0x0E00000000000000ULL;
}

constexpr U64 oo_mask(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x90ULL : 0x9000000000000000ULL;
}

constexpr U64 ooo_mask(Color c) {
    assert(valid_color(c));
    return (c == WHITE) ? 0x11ULL : 0x1100000000000000ULL;
}

inline Square lsb(const U64 b) {
    return Square(__builtin_ctzll(b));
}

inline int pop_count(U64 b) {
    return __builtin_popcountll(b);
}

inline Square pop_lsb(U64& b) {
    int n = lsb(b);
    b &= b - 1;
    return Square(n);
}

template <Direction d>
constexpr U64 shift(const U64 b) {
    switch (d) {
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

namespace bitboards {

template <int N>
struct SliderAttackTable {
    U64 mask[NUM_SQUARES]{};
    U64 magics[NUM_SQUARES]{};
    int shifts[NUM_SQUARES]{};
    U64 attacks[NUM_SQUARES][N]{};

    U64 idx(Square sq, U64 occ) const {
        assert(valid_sq(sq));
        return (occ & mask[sq]) * magics[sq] >> shifts[sq];
    }
};

extern SliderAttackTable<4096> ROOK_TABLE;
extern SliderAttackTable<512> BISHOP_TABLE;

extern U64 PAWN_ATTACKS[NUM_COLORS][NUM_SQUARES];
// doesnt include pawn attacks
extern U64 PSEUDO_ATTACKS[NUM_PIECE_TYPES][NUM_SQUARES];

extern U64 SQUARES_BETWEEN[NUM_SQUARES][NUM_SQUARES];
extern U64 LINE[NUM_SQUARES][NUM_SQUARES];

constexpr U64 valid_destination(Square sq, int step) {
    Square to = Square(int(sq) + step);
    return valid_sq(to) && std::abs(sq_file(sq) - sq_file(to)) <= 2 ? sq_bb(to) : 0;
};

constexpr U64 sliding_attack(PieceType pt, Square sq, U64 occ) {
    assert(pt == ROOK || pt == BISHOP);
    assert(valid_sq(sq));

    Direction root_dir[4] = {NORTH, SOUTH, EAST, WEST};
    Direction bishop_dir[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};

    U64 attacks = 0;
    for (auto d : (pt == ROOK ? root_dir : bishop_dir)) {
        Square s = sq;
        while (valid_destination(s, d)) {
            s += d;
            attacks |= sq_bb(s);
            if (occ & sq_bb(s))
                break;
        }
    }

    return attacks;
}

void init();

} // namespace bitboards

inline U64 between_bb(Square sq1, Square sq2) {
    assert(valid_sq(sq1) && valid_sq(sq2));
    return bitboards::SQUARES_BETWEEN[sq1][sq2];
}

inline U64 line(Square sq1, Square sq2) {
    assert(valid_sq(sq1) && valid_sq(sq2));
    return bitboards::LINE[sq1][sq2];
}

constexpr U64 pawn_attacks_bb(Color c, Square sq) {
    assert(valid_color(c) && valid_sq(sq));

    U64 b = sq_bb(sq);
    return (c == WHITE) ? shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b) //
                        : shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
}

constexpr U64 knight_attacks_bb(Square sq) {
    U64 b = 0;
    for (int step : {-17, -15, -10, -6, 6, 10, 15, 17})
        b |= bitboards::valid_destination(sq, step);
    return b;
}

inline U64 rook_attacks_bb(Square sq, const U64 occ) {
    assert(valid_sq(sq));

#if defined(__BMI2__)
    const int idx = _pext_u64(occ, bitboards::ROOK_TABLE.mask[sq]);
#else
    const U64 idx = bitboards::ROOK_TABLE.idx(sq, occ);
#endif

    return bitboards::ROOK_TABLE.attacks[sq][idx];
}

inline U64 bishop_attacks_bb(Square sq, const U64 occ) {
    assert(valid_sq(sq));

#if defined(__BMI2__)
    const int idx = _pext_u64(occ, bitboards::BISHOP_TABLE.mask[sq]);
#else
    const U64 idx = bitboards::BISHOP_TABLE.idx(sq, occ);
#endif

    return bitboards::BISHOP_TABLE.attacks[sq][idx];
}

constexpr U64 king_attacks_bb(Square sq) {
    U64 b = 0;
    for (int step : {-9, -8, -7, -1, 1, 7, 8, 9})
        b |= bitboards::valid_destination(sq, step);
    return b;
}

inline U64 attacks_bb(Color c, Square sq) {
    assert(valid_color(c) && valid_sq(sq));
    return bitboards::PAWN_ATTACKS[c][sq];
}

template <PieceType pt>
U64 attacks_bb(Square sq) {
    assert(valid_piece_type(pt) && valid_sq(sq) && pt != PAWN);
    return bitboards::PSEUDO_ATTACKS[pt][sq];
}

inline U64 attacks_bb(PieceType pt, Square sq) {
    assert(valid_piece_type(pt) && valid_sq(sq) && pt != PAWN);
    return bitboards::PSEUDO_ATTACKS[pt][sq];
}

template <PieceType pt>
U64 attacks_bb(Square sq, const U64 occ) {
    assert(valid_piece_type(pt) && valid_sq(sq) && occ && pt != PAWN);

    switch (pt) {
    case ROOK:
        return rook_attacks_bb(sq, occ);
    case BISHOP:
        return bishop_attacks_bb(sq, occ);
    case QUEEN:
        return rook_attacks_bb(sq, occ) | bishop_attacks_bb(sq, occ);
    default:
        return bitboards::PSEUDO_ATTACKS[pt][sq];
    }
}

inline U64 attacks_bb(PieceType pt, Square sq, const U64 occ) {
    assert(valid_piece_type(pt) && valid_sq(sq) && occ && pt != PAWN);

    switch (pt) {
    case ROOK:
        return attacks_bb<ROOK>(sq, occ);
    case BISHOP:
        return attacks_bb<BISHOP>(sq, occ);
    case QUEEN:
        return attacks_bb<QUEEN>(sq, occ);
    default:
        return bitboards::PSEUDO_ATTACKS[pt][sq];
    }
}

} // namespace chess
