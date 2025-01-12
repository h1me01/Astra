#include "zobrist.h"
#include "misc.h"

namespace Chess::Zobrist
{
    U64 psq[NUM_PIECES][NUM_SQUARES];
    U64 castle[16];
    U64 ep[8];
    U64 side;

    void init()
    {
        PRNG rng(1070372);

        for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
            for (int sq = a1; sq <= h8; sq++)
                psq[p][sq] = rng.rand<U64>();

        for (int f = FILE_A; f <= FILE_H; f++)
            ep[f] = rng.rand<U64>();

        castle[0] = 0;
        for (int i = 1; i < 16; i++)
            castle[i] = rng.rand<U64>();

        side = rng.rand<U64>();
    }

    U64 getPsq(Piece p, Square sq)
    {
        assert(p >= WHITE_PAWN && p <= BLACK_KING);
        assert(sq >= a1 && sq <= h8);
        return psq[p][sq];
    }

    U64 getCastle(int idx)
    {
        assert(idx >= 0 && idx < 16);
        return castle[idx];
    }

    U64 getEp(Square sq)
    {
        assert(sq >= a1 && sq <= h8);
        return ep[fileOf(sq)];
    }

    U64 getPawnZobrist(const Board &board)
    {
        U64 hash = 0;

        for (Color c : {WHITE, BLACK})
        {
            U64 pawns = board.getPieceBB(c, PAWN);
            while (pawns)
                hash ^= getPsq(makePiece(c, PAWN), popLsb(pawns));
        }

        return hash;
    }

    U64 getNonPawnZobrist(const Board &board, Color c)
    {
        U64 hash = 0;

        for (PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING})
        {
            U64 pieces = board.getPieceBB(c, pt);
            while (pieces)
                hash ^= getPsq(makePiece(c, pt), popLsb(pieces));
        }

        return hash;
    }

} // namespace Chess::Zobrist
