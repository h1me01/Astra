#include <cstring> // memcpy
#include "misc.h"
#include "attacks.h"

namespace Chess
{
    U64 SQUARES_BETWEEN[NUM_SQUARES][NUM_SQUARES];
    U64 LINE[NUM_SQUARES][NUM_SQUARES];

    U64 PSEUDO_LEGAL_ATTACKS[NUM_PIECE_TYPES][NUM_SQUARES];
    U64 ROOK_ATTACKS[NUM_SQUARES][4096];
    U64 BISHOP_ATTACKS[NUM_SQUARES][512];

    U64 slidingAttacks(Square sq, const U64 occ, const U64 mask)
    {
        assert(sq >= a1 && sq <= h8);
        // hyperbola quintessence algorithm
        const U64 mask_occ = mask & occ;
        return ((mask_occ - SQUARE_BB[sq] * 2) ^ (reverse(reverse(mask_occ) - reverse(SQUARE_BB[sq]) * 2))) & mask;
    }

    void initRookAttacks()
    {
        for (Square s = a1; s <= h8; ++s)
        {
            U64 blockers = 0;
            do
            {
#ifdef __BMI2__
                const int idx = _pext_u64(blockers, ROOK_MASKS[s]);
#else
                const U64 idx = (blockers * ROOK_MAGICS[s]) >> ROOK_SHIFTS[s];
#endif
                ROOK_ATTACKS[s][idx] = slidingAttacks(s, blockers, MASK_FILE[fileOf(s)]) | slidingAttacks(s, blockers, MASK_RANK[rankOf(s)]);
                blockers = (blockers - ROOK_MASKS[s]) & ROOK_MASKS[s];
            } while (blockers);
        }
    }

    void initBishopAttacks()
    {
        for (Square s = a1; s <= h8; ++s)
        {
            U64 blockers = 0;
            do
            {
#ifdef __BMI2__
                const int idx = _pext_u64(blockers, BISHOP_MASKS[s]);
#else
                const U64 idx = (blockers * BISHOP_MAGICS[s]) >> BISHOP_SHIFTS[s];
#endif
                BISHOP_ATTACKS[s][idx] = slidingAttacks(s, blockers, MASK_DIAGONAL[diagOf(s)]) | slidingAttacks(s, blockers, MASK_ANTI_DIAGONAL[antiDiagOf(s)]);
                blockers = (blockers - BISHOP_MASKS[s]) & BISHOP_MASKS[s];
            } while (blockers);
        }
    }

    void initLookUpTables()
    {
        initRookAttacks();
        initBishopAttacks();

        // init pseudo legal getAttacks
        for (Square s = a1; s <= h8; ++s)
        {
            PSEUDO_LEGAL_ATTACKS[KNIGHT][s] = KNIGHT_ATTACKS[s];
            PSEUDO_LEGAL_ATTACKS[KING][s] = KING_ATTACKS[s];
            PSEUDO_LEGAL_ATTACKS[ROOK][s] = getRookAttacks(s, 0);
            PSEUDO_LEGAL_ATTACKS[BISHOP][s] = getBishopAttacks(s, 0);
            PSEUDO_LEGAL_ATTACKS[QUEEN][s] = PSEUDO_LEGAL_ATTACKS[ROOK][s] | PSEUDO_LEGAL_ATTACKS[BISHOP][s];
        }

        // init squares between and line
        for (Square s1 = a1; s1 <= h8; ++s1)
        {
            for (Square s2 = a1; s2 <= h8; ++s2)
            {
                const U64 s = SQUARE_BB[s1] | SQUARE_BB[s2];

                if (fileOf(s1) == fileOf(s2) || rankOf(s1) == rankOf(s2))
                {
                    U64 b1 = getRookAttacks(s1, s);
                    U64 b2 = getRookAttacks(s2, s);
                    SQUARES_BETWEEN[s1][s2] = b1 & b2;

                    b1 = getRookAttacks(s1, 0);
                    b2 = getRookAttacks(s2, 0);
                    LINE[s1][s2] = (b1 & b2) | SQUARE_BB[s1] | SQUARE_BB[s2];
                }
                else if (diagOf(s1) == diagOf(s2) || antiDiagOf(s1) == antiDiagOf(s2))
                {
                    U64 b1 = getBishopAttacks(s1, s);
                    U64 b2 = getBishopAttacks(s2, s);
                    SQUARES_BETWEEN[s1][s2] = b1 & b2;

                    b1 = getBishopAttacks(s1, 0);
                    b2 = getBishopAttacks(s2, 0);
                    LINE[s1][s2] = (b1 & b2) | SQUARE_BB[s1] | SQUARE_BB[s2];
                }
            }
        }
    }

} // namespace Chess
