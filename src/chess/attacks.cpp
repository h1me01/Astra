#include <cstring> // memcpy
#include "attacks.h"
#include "misc.h"

namespace Chess
{
    U64 SQUARES_BETWEEN[NUM_SQUARES][NUM_SQUARES];
    U64 LINE[NUM_SQUARES][NUM_SQUARES];

    U64 PSEUDO_LEGAL_ATTACKS[NUM_PIECE_TYPES][NUM_SQUARES];

    U64 ROOK_ATTACK_MASKS[NUM_SQUARES];
    int ROOK_ATTACK_SHIFTS[NUM_SQUARES];
    U64 ROOK_ATTACKS[NUM_SQUARES][4096];

    U64 BISHOP_ATTACK_MASKS[NUM_SQUARES];
    int BISHOP_ATTACK_SHIFTS[NUM_SQUARES];
    U64 BISHOP_ATTACKS[NUM_SQUARES][512];

    U64 slidingAttacks(Square s, const U64 occ, const U64 mask)
    {
        // hyperbola quintessence algorithm
        const U64 mask_occ = mask & occ;
        return ((mask_occ - SQUARE_BB[s] * 2) ^ (reverse(reverse(mask_occ) - reverse(SQUARE_BB[s]) * 2))) & mask;
    }

    void initRookAttacks()
    {
        for (Square s = a1; s <= h8; ++s)
        {
            const U64 edges = ((MASK_RANK[FILE_A] | MASK_RANK[FILE_H]) & ~MASK_RANK[rankOf(s)]) |
                              ((MASK_FILE[FILE_A] | MASK_FILE[FILE_H]) & ~MASK_FILE[fileOf(s)]);
            ROOK_ATTACK_MASKS[s] = (MASK_RANK[rankOf(s)] ^ MASK_FILE[fileOf(s)]) & ~edges;
            ROOK_ATTACK_SHIFTS[s] = 64 - popCount(ROOK_ATTACK_MASKS[s]);

            U64 subset = 0;
            do
            {
                U64 idx = subset;
                idx = idx * ROOK_MAGICS[s];
                idx = idx >> ROOK_ATTACK_SHIFTS[s];
                ROOK_ATTACKS[s][idx] = slidingAttacks(s, subset, MASK_FILE[fileOf(s)]) | slidingAttacks(s, subset, MASK_RANK[rankOf(s)]);
                subset = (subset - ROOK_ATTACK_MASKS[s]) & ROOK_ATTACK_MASKS[s];
            } while (subset);
        }
    }

    void initBishopAttacks()
    {
        for (Square s = a1; s <= h8; ++s)
        {
            const U64 edges = ((MASK_RANK[FILE_A] | MASK_RANK[FILE_H]) & ~MASK_RANK[rankOf(s)]) |
                              ((MASK_FILE[FILE_A] | MASK_FILE[FILE_H]) & ~MASK_FILE[fileOf(s)]);
            BISHOP_ATTACK_MASKS[s] = (MASK_DIAGONAL[diagOf(s)] ^ MASK_ANTI_DIAGONAL[antiDiagOf(s)]) & ~edges;
            BISHOP_ATTACK_SHIFTS[s] = 64 - popCount(BISHOP_ATTACK_MASKS[s]);

            U64 subset = 0;
            do
            {
                U64 idx = subset;
                idx = idx * BISHOP_MAGICS[s];
                idx = idx >> BISHOP_ATTACK_SHIFTS[s];
                BISHOP_ATTACKS[s][idx] = slidingAttacks(s, subset, MASK_DIAGONAL[diagOf(s)]) | slidingAttacks(s, subset, MASK_ANTI_DIAGONAL[antiDiagOf(s)]);
                subset = (subset - BISHOP_ATTACK_MASKS[s]) & BISHOP_ATTACK_MASKS[s];
            } while (subset);
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
