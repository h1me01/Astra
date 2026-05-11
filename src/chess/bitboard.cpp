#include <cassert>
#include <cstring> // memcpy

#include "../ndarray.h"
#include "bitboard.h"
#include "magics.h"

namespace astra {

namespace bitboards {

namespace {

template <int N>
struct SliderAttackTable {
    NDArray<Bitboard, NUM_SQUARES> mask{};
    NDArray<Bitboard, NUM_SQUARES> magics{};
    NDArray<int, NUM_SQUARES> shifts{};
    NDArray<Bitboard, NUM_SQUARES, N> attacks{};

    Bitboard attacks_bb(Square sq, Bitboard occ) const {
        assert(valid_sq(sq));

#if defined(__BMI2__)
        int idx = _pext_u64(occ, mask(sq));
#else
        int idx = (occ & mask(sq)) * magics(sq) >> shifts(sq);
#endif
        return attacks(sq, idx);
    }
};

SliderAttackTable<4096> ROOK_TABLE;
SliderAttackTable<512> BISHOP_TABLE;

NDArray<Bitboard, NUM_PIECE_TYPES, NUM_SQUARES> PSEUDO_ATTACKS{};
NDArray<Bitboard, NUM_SQUARES, NUM_SQUARES> SQUARES_BETWEEN{};
NDArray<Bitboard, NUM_SQUARES, NUM_SQUARES> LINE{};

} // namespace

Bitboard sliding_attack(PieceType pt, Square sq, Bitboard occ) {
    assert(pt == ROOK || pt == BISHOP);
    assert(valid_sq(sq));

    Direction root_dir[4] = {NORTH, SOUTH, EAST, WEST};
    Direction bishop_dir[4] = {NORTH_EAST, SOUTH_EAST, SOUTH_WEST, NORTH_WEST};

    Bitboard attacks = 0;
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

template <typename Table>
void init_slider_table(Table& table, PieceType pt) {
    for (Square sq = SQ_A1; sq < NUM_SQUARES; ++sq) {
        Bitboard edges = ((rank_bb<RANK_1>() | rank_bb<RANK_8>()) & ~rank_bb(sq_rank(sq))) |
                         ((file_bb<FILE_A>() | file_bb<FILE_H>()) & ~file_bb(sq_file(sq)));

        table.mask(sq) = sliding_attack(pt, sq, 0) & ~edges;

        Bitboard blockers = 0;
        do {
#if defined(__BMI2__)
            const int idx = _pext_u64(blockers, table.mask(sq));
#else
            const int idx = (blockers * table.magics(sq)) >> table.shifts(sq);
#endif
            table.attacks(sq, idx) = sliding_attack(pt, sq, blockers);
            blockers = (blockers - table.mask(sq)) & table.mask(sq);
        } while (blockers);
    }
}

void init() {
    ROOK_TABLE.magics = magics::ROOK_MAGICS;
    ROOK_TABLE.shifts = magics::ROOK_SHIFTS;
    BISHOP_TABLE.magics = magics::BISHOP_MAGICS;
    BISHOP_TABLE.shifts = magics::BISHOP_SHIFTS;

    init_slider_table(ROOK_TABLE, ROOK);
    init_slider_table(BISHOP_TABLE, BISHOP);

    for (Square sq = SQ_A1; sq < NUM_SQUARES; ++sq) {
        PSEUDO_ATTACKS(KNIGHT, sq) = knight_attacks_bb(sq);
        PSEUDO_ATTACKS(BISHOP, sq) = bishop_attacks_bb(sq, 0);
        PSEUDO_ATTACKS(ROOK, sq) = rook_attacks_bb(sq, 0);
        PSEUDO_ATTACKS(QUEEN, sq) = PSEUDO_ATTACKS(ROOK, sq) | PSEUDO_ATTACKS(BISHOP, sq);
        PSEUDO_ATTACKS(KING, sq) = king_attacks_bb(sq);
    }

    for (Square sq1 = SQ_A1; sq1 < NUM_SQUARES; ++sq1) {
        for (PieceType pt : {BISHOP, ROOK}) {
            for (Square sq2 = SQ_A1; sq2 < NUM_SQUARES; ++sq2) {
                if (PSEUDO_ATTACKS(pt, sq1) & sq_bb(sq2)) {
                    LINE(sq1, sq2) = (attacks_bb(pt, sq1) & attacks_bb(pt, sq2)) | sq_bb(sq1) | sq_bb(sq2);
                    SQUARES_BETWEEN(sq1, sq2) = attacks_bb(pt, sq1, sq_bb(sq2)) & attacks_bb(pt, sq2, sq_bb(sq1));
                }
                SQUARES_BETWEEN(sq1, sq1) |= sq_bb(sq2);
            }
        }
    }
}

} // namespace bitboards

Bitboard between_bb(Square sq1, Square sq2) {
    assert(valid_sq(sq1));
    assert(valid_sq(sq2));
    return bitboards::SQUARES_BETWEEN(sq1, sq2);
}

Bitboard line(Square sq1, Square sq2) {
    assert(valid_sq(sq1));
    assert(valid_sq(sq2));
    return bitboards::LINE(sq1, sq2);
}

Bitboard rook_attacks_bb(Square sq, const Bitboard occ) {
    assert(valid_sq(sq));
    return bitboards::ROOK_TABLE.attacks_bb(sq, occ);
}

Bitboard bishop_attacks_bb(Square sq, const Bitboard occ) {
    assert(valid_sq(sq));
    return bitboards::BISHOP_TABLE.attacks_bb(sq, occ);
}

template <PieceType pt>
Bitboard attacks_bb(Square sq) {
    assert(pt != PAWN);
    assert(valid_sq(sq));
    assert(valid_piece_type(pt));
    return bitboards::PSEUDO_ATTACKS(pt, sq);
}

template Bitboard attacks_bb<KNIGHT>(Square sq);
template Bitboard attacks_bb<BISHOP>(Square sq);
template Bitboard attacks_bb<ROOK>(Square sq);
template Bitboard attacks_bb<QUEEN>(Square sq);
template Bitboard attacks_bb<KING>(Square sq);

Bitboard attacks_bb(PieceType pt, Square sq) {
    assert(pt != PAWN);
    assert(valid_sq(sq));
    assert(valid_piece_type(pt));
    return bitboards::PSEUDO_ATTACKS(pt, sq);
}

} // namespace astra
