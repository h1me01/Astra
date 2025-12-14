#include <cassert>
#include <cstring> // memcpy

#ifdef __BMI2__
#include <immintrin.h>
#endif

#include "bitboard.h"
#include "misc.h"

namespace chess {

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
    0x0a80004000801220ULL, 0x8040004010002008ULL, 0x2080200010008008ULL, 0x1100100008210004ULL, //
    0xc200209084020008ULL, 0x2100010004000208ULL, 0x0400081000822421ULL, 0x0200010422048844ULL, //
    0x0800800080400024ULL, 0x0001402000401000ULL, 0x3000801000802001ULL, 0x4400800800100083ULL, //
    0x0904802402480080ULL, 0x4040800400020080ULL, 0x0018808042000100ULL, 0x4040800080004100ULL, //
    0x0040048001458024ULL, 0x00a0004000205000ULL, 0x3100808010002000ULL, 0x4825010010000820ULL, //
    0x5004808008000401ULL, 0x2024818004000a00ULL, 0x0005808002000100ULL, 0x2100060004806104ULL, //
    0x0080400880008421ULL, 0x4062220600410280ULL, 0x010a004a00108022ULL, 0x0000100080080080ULL, //
    0x0021000500080010ULL, 0x0044000202001008ULL, 0x0000100400080102ULL, 0xc020128200040545ULL, //
    0x0080002000400040ULL, 0x0000804000802004ULL, 0x0000120022004080ULL, 0x010a386103001001ULL, //
    0x9010080080800400ULL, 0x8440020080800400ULL, 0x0004228824001001ULL, 0x000000490a000084ULL, //
    0x0080002000504000ULL, 0x200020005000c000ULL, 0x0012088020420010ULL, 0x0010010080080800ULL, //
    0x0085001008010004ULL, 0x0002000204008080ULL, 0x0040413002040008ULL, 0x0000304081020004ULL, //
    0x0080204000800080ULL, 0x3008804000290100ULL, 0x1010100080200080ULL, 0x2008100208028080ULL, //
    0x5000850800910100ULL, 0x8402019004680200ULL, 0x0120911028020400ULL, 0x0000008044010200ULL, //
    0x0020850200244012ULL, 0x0020850200244012ULL, 0x0000102001040841ULL, 0x140900040a100021ULL, //
    0x000200282410a102ULL, 0x000200282410a102ULL, 0x000200282410a102ULL, 0x4048240043802106ULL, //
};

constexpr int ROOK_SHIFTS[NUM_SQUARES] = {
    52, 53, 53, 53, 53, 53, 53, 52, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    53, 54, 54, 54, 54, 54, 54, 53, //
    52, 53, 53, 53, 53, 53, 53, 52, //
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
    0x40106000a1160020ULL, 0x0020010250810120ULL, 0x2010010220280081ULL, 0x002806004050c040ULL, //
    0x0002021018000000ULL, 0x2001112010000400ULL, 0x0881010120218080ULL, 0x1030820110010500ULL, //
    0x0000120222042400ULL, 0x2000020404040044ULL, 0x8000480094208000ULL, 0x0003422a02000001ULL, //
    0x000a220210100040ULL, 0x8004820202226000ULL, 0x0018234854100800ULL, 0x0100004042101040ULL, //
    0x0004001004082820ULL, 0x0010000810010048ULL, 0x1014004208081300ULL, 0x2080818802044202ULL, //
    0x0040880c00a00100ULL, 0x0080400200522010ULL, 0x0001000188180b04ULL, 0x0080249202020204ULL, //
    0x1004400004100410ULL, 0x00013100a0022206ULL, 0x2148500001040080ULL, 0x4241080011004300ULL, //
    0x4020848004002000ULL, 0x10101380d1004100ULL, 0x0008004422020284ULL, 0x01010a1041008080ULL, //
    0x0808080400082121ULL, 0x0808080400082121ULL, 0x0091128200100c00ULL, 0x0202200802010104ULL, //
    0x8c0a020200440085ULL, 0x01a0008080b10040ULL, 0x0889520080122800ULL, 0x100902022202010aULL, //
    0x04081a0816002000ULL, 0x0000681208005000ULL, 0x8170840041008802ULL, 0x0a00004200810805ULL, //
    0x0830404408210100ULL, 0x2602208106006102ULL, 0x1048300680802628ULL, 0x2602208106006102ULL, //
    0x0602010120110040ULL, 0x0941010801043000ULL, 0x000040440a210428ULL, 0x0008240020880021ULL, //
    0x0400002012048200ULL, 0x00ac102001210220ULL, 0x0220021002009900ULL, 0x84440c080a013080ULL, //
    0x0001008044200440ULL, 0x0004c04410841000ULL, 0x2000500104011130ULL, 0x1a0c010011c20229ULL, //
    0x0044800112202200ULL, 0x0434804908100424ULL, 0x0300404822c08200ULL, 0x48081010008a2a80ULL, //
};

constexpr int BISHOP_SHIFTS[NUM_SQUARES] = {
    58, 59, 59, 59, 59, 59, 59, 58, //
    59, 59, 59, 59, 59, 59, 59, 59, //
    59, 59, 57, 57, 57, 57, 59, 59, //
    59, 59, 57, 55, 55, 57, 59, 59, //
    59, 59, 57, 55, 55, 57, 59, 59, //
    59, 59, 57, 57, 57, 57, 59, 59, //
    59, 59, 59, 59, 59, 59, 59, 59, //
    58, 59, 59, 59, 59, 59, 59, 58, //
};

constexpr U64 DIAGONAL_MASK[15] = {
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

constexpr U64 ANTI_DIAGONAL_MASK[15] = {
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

U64 SQUARES_BETWEEN[NUM_SQUARES][NUM_SQUARES];
U64 LINE[NUM_SQUARES][NUM_SQUARES];

U64 PAWN_ATTACKS[NUM_COLORS][NUM_SQUARES];
U64 ROOK_ATTACKS[NUM_SQUARES][4096];
U64 BISHOP_ATTACKS[NUM_SQUARES][512];

U64 PSEUDO_LEGAL_ATTACKS[NUM_PIECE_TYPES][NUM_SQUARES];

// helper functions

constexpr U64 diag_mask(int idx) {
    assert(idx >= 0 && idx < 15);
    return DIAGONAL_MASK[idx];
}

constexpr U64 anti_diag_mask(int idx) {
    assert(idx >= 0 && idx < 15);
    return ANTI_DIAGONAL_MASK[idx];
}

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
    return ((mask_occ - sq_bb(sq) * 2) ^ (reverse_bb(reverse_bb(mask_occ) - reverse_bb(sq_bb(sq)) * 2))) & mask;
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
            ROOK_ATTACKS[sq][idx] = sliding_attacks(sq, blockers, file_mask(sq_file(sq))) |
                                    sliding_attacks(sq, blockers, rank_mask(sq_rank(sq)));
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
            BISHOP_ATTACKS[sq][idx] = sliding_attacks(sq, blockers, diag_mask(sq_diag(sq))) |
                                      sliding_attacks(sq, blockers, anti_diag_mask(sq_anti_diag(sq)));
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

namespace bitboards {

void init() {
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
        const U64 b = sq_bb(sq);
        if(c == WHITE)
            return shift<NORTH_WEST>(b) | shift<NORTH_EAST>(b);
        else
            return shift<SOUTH_WEST>(b) | shift<SOUTH_EAST>(b);
    };

    // init pseudo legal attacks
    for(Square sq = a1; sq <= h8; ++sq) {
        PAWN_ATTACKS[WHITE][sq] = set_pawn_attacks(WHITE, sq);
        PAWN_ATTACKS[BLACK][sq] = set_pawn_attacks(BLACK, sq);

        for(int i = 0; i < 8; ++i) {
            Square king_sq = Square(int(sq) + king_offsets[i]);
            Square knight_sq = Square(int(sq) + knight_offsets[i]);

            if(in_bounds(sq, king_sq, 1))
                PSEUDO_LEGAL_ATTACKS[KING][sq] |= sq_bb(king_sq);
            if(in_bounds(sq, knight_sq, 2))
                PSEUDO_LEGAL_ATTACKS[KNIGHT][sq] |= sq_bb(knight_sq);
        }

        PSEUDO_LEGAL_ATTACKS[BISHOP][sq] = get_bishop_attacks(sq);
        PSEUDO_LEGAL_ATTACKS[ROOK][sq] = get_rook_attacks(sq);
        PSEUDO_LEGAL_ATTACKS[QUEEN][sq] = PSEUDO_LEGAL_ATTACKS[ROOK][sq] | PSEUDO_LEGAL_ATTACKS[BISHOP][sq];
    }

    // init squares between and line
    for(Square sq1 = a1; sq1 <= h8; ++sq1) {
        for(Square sq2 = a1; sq2 <= h8; ++sq2) {
            const U64 s = sq_bb(sq1) | sq_bb(sq2);

            if(sq_file(sq1) == sq_file(sq2) || sq_rank(sq1) == sq_rank(sq2)) {
                U64 b1 = get_rook_attacks(sq1, s);
                U64 b2 = get_rook_attacks(sq2, s);
                SQUARES_BETWEEN[sq1][sq2] = b1 & b2;

                b1 = get_rook_attacks(sq1);
                b2 = get_rook_attacks(sq2);
                LINE[sq1][sq2] = (b1 & b2) | sq_bb(sq1) | sq_bb(sq2);
            } else if(sq_diag(sq1) == sq_diag(sq2) || sq_anti_diag(sq1) == sq_anti_diag(sq2)) {
                U64 b1 = get_bishop_attacks(sq1, s);
                U64 b2 = get_bishop_attacks(sq2, s);
                SQUARES_BETWEEN[sq1][sq2] = b1 & b2;

                b1 = get_bishop_attacks(sq1);
                b2 = get_bishop_attacks(sq2);
                LINE[sq1][sq2] = (b1 & b2) | sq_bb(sq1) | sq_bb(sq2);
            }
        }
    }
}

} // namespace bitboards

} // namespace chess
