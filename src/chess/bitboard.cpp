#include <cassert>
#include <cstring> // memcpy

#ifdef __BMI2__
#include <immintrin.h>
#endif

#include "bitboard.h"
#include "misc.h"

namespace Chess {

// variables

constexpr U64 ROOK_MASKS[64] = {
    0x000101010101017eULL, 0x000202020202027cULL, 0x000404040404047aULL, 0x0008080808080876ULL, //
    0x001010101010106eULL, 0x002020202020205eULL, 0x004040404040403eULL, 0x008080808080807eULL, //
    0x0001010101017e00ULL, 0x0002020202027c00ULL, 0x0004040404047a00ULL, 0x0008080808087600ULL, //
    0x0010101010106e00ULL, 0x0020202020205e00ULL, 0x0040404040403e00ULL, 0x0080808080807e00ULL, //
    0x00010101017e0100ULL, 0x00020202027c0200ULL, 0x00040404047a0400ULL, 0x0008080808760800ULL, //
    0x00101010106e1000ULL, 0x00202020205e2000ULL, 0x00404040403e4000ULL, 0x00808080807e8000ULL, //
    0x000101017e010100ULL, 0x000202027c020200ULL, 0x000404047a040400ULL, 0x0008080876080800ULL, //
    0x001010106e101000ULL, 0x002020205e202000ULL, 0x004040403e404000ULL, 0x008080807e808000ULL, //
    0x0001017e01010100ULL, 0x0002027c02020200ULL, 0x0004047a04040400ULL, 0x0008087608080800ULL, //
    0x0010106e10101000ULL, 0x0020205e20202000ULL, 0x0040403e40404000ULL, 0x0080807e80808000ULL, //
    0x00017e0101010100ULL, 0x00027c0202020200ULL, 0x00047a0404040400ULL, 0x0008760808080800ULL, //
    0x00106e1010101000ULL, 0x00205e2020202000ULL, 0x00403e4040404000ULL, 0x00807e8080808000ULL, //
    0x007e010101010100ULL, 0x007c020202020200ULL, 0x007a040404040400ULL, 0x0076080808080800ULL, //
    0x006e101010101000ULL, 0x005e202020202000ULL, 0x003e404040404000ULL, 0x007e808080808000ULL, //
    0x7e01010101010100ULL, 0x7c02020202020200ULL, 0x7a04040404040400ULL, 0x7608080808080800ULL, //
    0x6e10101010101000ULL, 0x5e20202020202000ULL, 0x3e40404040404000ULL, 0x7e80808080808000ULL  //
};

constexpr U64 ROOK_MAGICS[NUM_SQUARES] = {
    0x0080001020400080ULL, 0x0040001000200040ULL, 0x0080081000200080ULL, 0x0080040800100080ULL, //
    0x0080020400080080ULL, 0x0080010200040080ULL, 0x0080008001000200ULL, 0x0080002040800100ULL, //
    0x0000800020400080ULL, 0x0000400020005000ULL, 0x0000801000200080ULL, 0x0000800800100080ULL, //
    0x0000800400080080ULL, 0x0000800200040080ULL, 0x0000800100020080ULL, 0x0000800040800100ULL, //
    0x0000208000400080ULL, 0x0000404000201000ULL, 0x0000808010002000ULL, 0x0000808008001000ULL, //
    0x0000808004000800ULL, 0x0000808002000400ULL, 0x0000010100020004ULL, 0x0000020000408104ULL, //
    0x0000208080004000ULL, 0x0000200040005000ULL, 0x0000100080200080ULL, 0x0000080080100080ULL, //
    0x0000040080080080ULL, 0x0000020080040080ULL, 0x0000010080800200ULL, 0x0000800080004100ULL, //
    0x0000204000800080ULL, 0x0000200040401000ULL, 0x0000100080802000ULL, 0x0000080080801000ULL, //
    0x0000040080800800ULL, 0x0000020080800400ULL, 0x0000020001010004ULL, 0x0000800040800100ULL, //
    0x0000204000808000ULL, 0x0000200040008080ULL, 0x0000100020008080ULL, 0x0000080010008080ULL, //
    0x0000040008008080ULL, 0x0000020004008080ULL, 0x0000010002008080ULL, 0x0000004081020004ULL, //
    0x0000204000800080ULL, 0x0000200040008080ULL, 0x0000100020008080ULL, 0x0000080010008080ULL, //
    0x0000040008008080ULL, 0x0000020004008080ULL, 0x0000800100020080ULL, 0x0000800041000080ULL, //
    0x00FFFCDDFCED714AULL, 0x007FFCDDFCED714AULL, 0x003FFFCDFFD88096ULL, 0x0000040810002101ULL, //
    0x0001000204080011ULL, 0x0001000204000801ULL, 0x0001000082000401ULL, 0x0001FFFAABFAD1A2ULL  //
};

constexpr int ROOK_SHIFTS[NUM_SQUARES] = {
    52, 53, 53, 53, 53, 53, 53, 52, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    52, 53, 53, 53, 53, 53, 53, 52  //
};

constexpr U64 BISHOP_MASKS[64] = {
    0x0040201008040200ULL, 0x0000402010080400ULL, 0x0000004020100a00ULL, 0x0000000040221400ULL, //
    0x0000000002442800ULL, 0x0000000204085000ULL, 0x0000020408102000ULL, 0x0002040810204000ULL, //
    0x0020100804020000ULL, 0x0040201008040000ULL, 0x00004020100a0000ULL, 0x0000004022140000ULL, //
    0x0000000244280000ULL, 0x0000020408500000ULL, 0x0002040810200000ULL, 0x0004081020400000ULL, //
    0x0010080402000200ULL, 0x0020100804000400ULL, 0x004020100a000a00ULL, 0x0000402214001400ULL, //
    0x0000024428002800ULL, 0x0002040850005000ULL, 0x0004081020002000ULL, 0x0008102040004000ULL, //
    0x0008040200020400ULL, 0x0010080400040800ULL, 0x0020100a000a1000ULL, 0x0040221400142200ULL, //
    0x0002442800284400ULL, 0x0004085000500800ULL, 0x0008102000201000ULL, 0x0010204000402000ULL, //
    0x0004020002040800ULL, 0x0008040004081000ULL, 0x00100a000a102000ULL, 0x0022140014224000ULL, //
    0x0044280028440200ULL, 0x0008500050080400ULL, 0x0010200020100800ULL, 0x0020400040201000ULL, //
    0x0002000204081000ULL, 0x0004000408102000ULL, 0x000a000a10204000ULL, 0x0014001422400000ULL, //
    0x0028002844020000ULL, 0x0050005008040200ULL, 0x0020002010080400ULL, 0x0040004020100800ULL, //
    0x0000020408102000ULL, 0x0000040810204000ULL, 0x00000a1020400000ULL, 0x0000142240000000ULL, //
    0x0000284402000000ULL, 0x0000500804020000ULL, 0x0000201008040200ULL, 0x0000402010080400ULL, //
    0x0002040810204000ULL, 0x0004081020400000ULL, 0x000a102040000000ULL, 0x0014224000000000ULL, //
    0x0028440200000000ULL, 0x0050080402000000ULL, 0x0020100804020000ULL, 0x0040201008040200ULL  //
};

constexpr U64 BISHOP_MAGICS[NUM_SQUARES] = {
    0x0002020202020200ULL, 0x0002020202020000ULL, 0x0004010202000000ULL, 0x0004040080000000ULL, //
    0x0001104000000000ULL, 0x0000821040000000ULL, 0x0000410410400000ULL, 0x0000104104104000ULL, //
    0x0000040404040400ULL, 0x0000020202020200ULL, 0x0000040102020000ULL, 0x0000040400800000ULL, //
    0x0000011040000000ULL, 0x0000008210400000ULL, 0x0000004104104000ULL, 0x0000002082082000ULL, //
    0x0004000808080800ULL, 0x0002000404040400ULL, 0x0001000202020200ULL, 0x0000800802004000ULL, //
    0x0000800400A00000ULL, 0x0000200100884000ULL, 0x0000400082082000ULL, 0x0000200041041000ULL, //
    0x0002080010101000ULL, 0x0001040008080800ULL, 0x0000208004010400ULL, 0x0000404004010200ULL, //
    0x0000840000802000ULL, 0x0000404002011000ULL, 0x0000808001041000ULL, 0x0000404000820800ULL, //
    0x0001041000202000ULL, 0x0000820800101000ULL, 0x0000104400080800ULL, 0x0000020080080080ULL, //
    0x0000404040040100ULL, 0x0000808100020100ULL, 0x0001010100020800ULL, 0x0000808080010400ULL, //
    0x0000820820004000ULL, 0x0000410410002000ULL, 0x0000082088001000ULL, 0x0000002011000800ULL, //
    0x0000080100400400ULL, 0x0001010101000200ULL, 0x0002020202000400ULL, 0x0001010101000200ULL, //
    0x0000410410400000ULL, 0x0000208208200000ULL, 0x0000002084100000ULL, 0x0000000020880000ULL, //
    0x0000001002020000ULL, 0x0000040408020000ULL, 0x0004040404040000ULL, 0x0002020202020000ULL, //
    0x0000104104104000ULL, 0x0000002082082000ULL, 0x0000000020841000ULL, 0x0000000000208800ULL, //
    0x0000000010020200ULL, 0x0000000404080200ULL, 0x0000040404040400ULL, 0x0002020202020200ULL  //
};

constexpr int BISHOP_SHIFTS[NUM_SQUARES] = {
    58, 59, 59, 59, 59, 59, 59, 58, //
    59, 59, 59, 59, 59, 59, 59, 59, //
    59, 59, 57, 57, 57, 57, 59, 59, //
    59, 59, 57, 55, 55, 57, 59, 59, //
    59, 59, 57, 55, 55, 57, 59, 59, //
    59, 59, 57, 57, 57, 57, 59, 59, //
    59, 59, 59, 59, 59, 59, 59, 59, //
    58, 59, 59, 59, 59, 59, 59, 58  //
};

U64 SQUARES_BETWEEN[NUM_SQUARES][NUM_SQUARES];
U64 LINE[NUM_SQUARES][NUM_SQUARES];

U64 PAWN_ATTACKS[NUM_COLORS][NUM_SQUARES];
U64 ROOK_ATTACKS[NUM_SQUARES][4096];
U64 BISHOP_ATTACKS[NUM_SQUARES][512];

U64 PSEUDO_LEGAL_ATTACKS[NUM_PIECE_TYPES][NUM_SQUARES];

// helper functions

U64 reverse_bb(U64 b) {
    b = ((b & 0x5555555555555555) << 1) | ((b >> 1) & 0x5555555555555555);
    b = ((b & 0x3333333333333333) << 2) | ((b >> 2) & 0x3333333333333333);
    b = ((b & 0x0f0f0f0f0f0f0f0f) << 4) | ((b >> 4) & 0x0f0f0f0f0f0f0f0f);
    b = ((b & 0x00ff00ff00ff00ff) << 8) | ((b >> 8) & 0x00ff00ff00ff00ff);
    return (b << 48) | ((b & 0xffff0000) << 16) | ((b >> 16) & 0xffff0000) | (b >> 48);
}

U64 sliding_attacks(Square sq, const U64 occ, const U64 mask) {
    assert(valid_sq(sq));
    // hyperbola quintessence algorithm
    const U64 mask_occ = mask & occ;
    return ((mask_occ - square_bb(sq) * 2) ^ (reverse_bb(reverse_bb(mask_occ) - reverse_bb(square_bb(sq)) * 2))) & mask;
}

void init_rook_attacks() {
    for(Square sq = a1; sq <= h8; ++sq) {
        U64 blockers = 0;
        do {
#ifdef __BMI2__
            const int idx = _pext_u64(blockers, ROOK_MASKS[sq]);
#else
            const U64 idx = (blockers * ROOK_MAGICS[sq]) >> ROOK_SHIFTS[sq];
#endif
            ROOK_ATTACKS[sq][idx] = sliding_attacks(sq, blockers, MASK_FILE[sq_file(sq)]) |
                                    sliding_attacks(sq, blockers, MASK_RANK[sq_rank(sq)]);
            blockers = (blockers - ROOK_MASKS[sq]) & ROOK_MASKS[sq];
        } while(blockers);
    }
}

U64 get_rook_attacks(Square sq, const U64 occ = 0) {
    assert(valid_sq(sq));

#ifdef __BMI2__
    const int idx = _pext_u64(occ, ROOK_MASKS[sq]);
#else
    const U64 idx = (occ & ROOK_MASKS[sq]) * ROOK_MAGICS[sq] >> ROOK_SHIFTS[sq];
#endif

    return ROOK_ATTACKS[sq][idx];
}

void init_bishop_attacks() {
    for(Square sq = a1; sq <= h8; ++sq) {
        U64 blockers = 0;
        do {
#ifdef __BMI2__
            const int idx = _pext_u64(blockers, BISHOP_MASKS[sq]);
#else
            const U64 idx = (blockers * BISHOP_MAGICS[sq]) >> BISHOP_SHIFTS[sq];
#endif
            BISHOP_ATTACKS[sq][idx] = sliding_attacks(sq, blockers, MASK_DIAGONAL[sq_diag(sq)]) |
                                      sliding_attacks(sq, blockers, MASK_ANTI_DIAGONAL[sq_antidiag(sq)]);
            blockers = (blockers - BISHOP_MASKS[sq]) & BISHOP_MASKS[sq];
        } while(blockers);
    }
}

U64 get_bishop_attacks(Square sq, const U64 occ = 0) {
    assert(valid_sq(sq));

#ifdef __BMI2__
    const int idx = _pext_u64(occ, BISHOP_MASKS[sq]);
#else
    const U64 idx = (occ & BISHOP_MASKS[sq]) * BISHOP_MAGICS[sq] >> BISHOP_SHIFTS[sq];
#endif

    return BISHOP_ATTACKS[sq][idx];
}

// actual used functions

void init_lookup_tables() {
    init_rook_attacks();
    init_bishop_attacks();

    const int king_offsets[8] = {-9, -8, -7, -1, 1, 7, 8, 9};
    const int knight_offsets[8] = {-17, -15, -10, -6, 6, 10, 15, 17};

    // helper
    auto in_bounds = [](Square sq, Square target_sq, int max_delta) {
        return valid_sq(target_sq) &&                                     //
               std::abs(sq_rank(sq) - sq_rank(target_sq)) <= max_delta && //
               std::abs(sq_file(sq) - sq_file(target_sq)) <= max_delta;
    };

    auto set_pawn_attacks = [](Color c, Square sq) {
        const U64 sq_bb = square_bb(sq);
        if(c == WHITE)
            return shift<NORTH_WEST>(sq_bb) | shift<NORTH_EAST>(sq_bb);
        else
            return shift<SOUTH_WEST>(sq_bb) | shift<SOUTH_EAST>(sq_bb);
    };

    // init pseudo legal attacks
    for(Square sq = a1; sq <= h8; ++sq) {
        PAWN_ATTACKS[WHITE][sq] = set_pawn_attacks(WHITE, sq);
        PAWN_ATTACKS[BLACK][sq] = set_pawn_attacks(BLACK, sq);

        for(int i = 0; i < 8; ++i) {
            Square king_sq = Square(int(sq) + king_offsets[i]);
            Square knight_sq = Square(int(sq) + knight_offsets[i]);

            if(in_bounds(sq, king_sq, 1))
                PSEUDO_LEGAL_ATTACKS[KING][sq] |= square_bb(king_sq);
            if(in_bounds(sq, knight_sq, 2))
                PSEUDO_LEGAL_ATTACKS[KNIGHT][sq] |= square_bb(knight_sq);
        }

        PSEUDO_LEGAL_ATTACKS[BISHOP][sq] = get_bishop_attacks(sq);
        PSEUDO_LEGAL_ATTACKS[ROOK][sq] = get_rook_attacks(sq);
        PSEUDO_LEGAL_ATTACKS[QUEEN][sq] = PSEUDO_LEGAL_ATTACKS[ROOK][sq] | PSEUDO_LEGAL_ATTACKS[BISHOP][sq];
    }

    // init squares between and line
    for(Square sq1 = a1; sq1 <= h8; ++sq1) {
        for(Square sq2 = a1; sq2 <= h8; ++sq2) {
            const U64 s = square_bb(sq1) | square_bb(sq2);

            if(sq_file(sq1) == sq_file(sq2) || sq_rank(sq1) == sq_rank(sq2)) {
                U64 b1 = get_rook_attacks(sq1, s);
                U64 b2 = get_rook_attacks(sq2, s);
                SQUARES_BETWEEN[sq1][sq2] = b1 & b2;

                b1 = get_rook_attacks(sq1);
                b2 = get_rook_attacks(sq2);
                LINE[sq1][sq2] = (b1 & b2) | square_bb(sq1) | square_bb(sq2);
            } else if(sq_diag(sq1) == sq_diag(sq2) || sq_antidiag(sq1) == sq_antidiag(sq2)) {
                U64 b1 = get_bishop_attacks(sq1, s);
                U64 b2 = get_bishop_attacks(sq2, s);
                SQUARES_BETWEEN[sq1][sq2] = b1 & b2;

                b1 = get_bishop_attacks(sq1);
                b2 = get_bishop_attacks(sq2);
                LINE[sq1][sq2] = (b1 & b2) | square_bb(sq1) | square_bb(sq2);
            }
        }
    }
}

U64 between_bb(Square sq1, Square sq2) {
    assert(valid_sq(sq1));
    assert(valid_sq(sq2));
    return SQUARES_BETWEEN[sq1][sq2];
}

U64 line(Square sq1, Square sq2) {
    assert(valid_sq(sq1));
    assert(valid_sq(sq2));
    return LINE[sq1][sq2];
}

U64 get_pawn_attacks(Color c, Square sq) {
    assert(valid_sq(sq));
    assert(valid_color(c));
    return PAWN_ATTACKS[c][sq];
}

U64 get_attacks(PieceType pt, Square sq, const U64 occ) {
    assert(valid_sq(sq));
    assert(valid_piece_type(pt) && pt != PAWN);

    switch(pt) {
    case ROOK:
        return get_rook_attacks(sq, occ);
    case BISHOP:
        return get_bishop_attacks(sq, occ);
    case QUEEN:
        return get_rook_attacks(sq, occ) | get_bishop_attacks(sq, occ);
    case KNIGHT:
    case KING:
        return PSEUDO_LEGAL_ATTACKS[pt][sq];
    default:
        return 0;
    }
}

} // namespace Chess
