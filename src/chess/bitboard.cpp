#include <cassert>
#include <cstring> // memcpy

#include "bitboard.h"
#include "magics.h"
#include "misc.h"

namespace chess {

namespace bitboards {

SliderAttackTable<4096> ROOK_TABLE;
SliderAttackTable<512> BISHOP_TABLE;

U64 PAWN_ATTACKS[NUM_COLORS][NUM_SQUARES];
U64 PSEUDO_ATTACKS[NUM_PIECE_TYPES][NUM_SQUARES];

U64 SQUARES_BETWEEN[NUM_SQUARES][NUM_SQUARES];
U64 LINE[NUM_SQUARES][NUM_SQUARES];

void init_slider_attacks(PieceType pt) {
    for (Square sq = a1; sq <= h8; ++sq) {
        U64 edges = ((rank_mask<RANK_1>() | rank_mask<RANK_8>()) & ~rank_mask(sq_rank(sq))) |
                    ((file_mask<FILE_A>() | file_mask<FILE_H>()) & ~file_mask(sq_file(sq)));
        U64 mask = bitboards::sliding_attack(pt, sq, 0) & ~edges;

        if (pt == ROOK)
            ROOK_TABLE.mask[sq] = mask;
        else
            BISHOP_TABLE.mask[sq] = mask;

        U64 blockers = 0;
        do {
#if defined(__BMI2__)
            const int idx = _pext_u64(blockers, mask);
#else
            U64 magic = (pt == ROOK) ? ROOK_TABLE.magics[sq] : BISHOP_TABLE.magics[sq];
            int shift = (pt == ROOK) ? ROOK_TABLE.shifts[sq] : BISHOP_TABLE.shifts[sq];
            const U64 idx = (blockers * magic) >> shift;
#endif

            U64 attack = bitboards::sliding_attack(pt, sq, blockers);
            if (pt == ROOK)
                ROOK_TABLE.attacks[sq][idx] = attack;
            else
                BISHOP_TABLE.attacks[sq][idx] = attack;

            blockers = (blockers - mask) & mask;
        } while (blockers);
    }
}

void init() {
    std::memcpy(ROOK_TABLE.magics, magics::ROOK_MAGICS, sizeof(magics::ROOK_MAGICS));
    std::memcpy(ROOK_TABLE.shifts, magics::ROOK_SHIFTS, sizeof(magics::ROOK_SHIFTS));
    std::memcpy(BISHOP_TABLE.magics, magics::BISHOP_MAGICS, sizeof(magics::BISHOP_MAGICS));
    std::memcpy(BISHOP_TABLE.shifts, magics::BISHOP_SHIFTS, sizeof(magics::BISHOP_SHIFTS));

    init_slider_attacks(ROOK);
    init_slider_attacks(BISHOP);

    for (Square sq = a1; sq <= h8; ++sq) {
        PAWN_ATTACKS[WHITE][sq] = pawn_attacks_bb(WHITE, sq);
        PAWN_ATTACKS[BLACK][sq] = pawn_attacks_bb(BLACK, sq);
        PSEUDO_ATTACKS[KNIGHT][sq] = knight_attacks_bb(sq);
        PSEUDO_ATTACKS[BISHOP][sq] = bishop_attacks_bb(sq, 0);
        PSEUDO_ATTACKS[ROOK][sq] = rook_attacks_bb(sq, 0);
        PSEUDO_ATTACKS[QUEEN][sq] = PSEUDO_ATTACKS[ROOK][sq] | PSEUDO_ATTACKS[BISHOP][sq];
        PSEUDO_ATTACKS[KING][sq] = king_attacks_bb(sq);
    }

    for (Square sq1 = a1; sq1 <= h8; ++sq1) {
        for (PieceType pt : {BISHOP, ROOK}) {
            for (Square sq2 = a1; sq2 <= h8; ++sq2) {
                if (PSEUDO_ATTACKS[pt][sq1] & sq_bb(sq2)) {
                    LINE[sq1][sq2] = (attacks_bb(pt, sq1) & attacks_bb(pt, sq2)) | sq_bb(sq1) | sq_bb(sq2);
                    SQUARES_BETWEEN[sq1][sq2] = attacks_bb(pt, sq1, sq_bb(sq2)) & attacks_bb(pt, sq2, sq_bb(sq1));
                }
                SQUARES_BETWEEN[sq1][sq1] |= sq_bb(sq2);
            }
        }
    }
}

} // namespace bitboards

} // namespace chess
