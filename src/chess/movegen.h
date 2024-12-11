#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"

namespace Chess
{
    constexpr U64 ooBlockersMask(Color c) { return c == WHITE ? 0x60 : 0x6000000000000000; }
    constexpr U64 oooBlockersMask(Color c) { return c == WHITE ? 0xe : 0xE00000000000000; }
    constexpr U64 ignoreOOODanger(Color c) { return c == WHITE ? 0x2 : 0x200000000000000; }

    // helper to generate quiet/capture moves
    inline Move *make(Move *moves, Square from, U64 to)
    {
        while (to)
            *moves++ = Move(from, popLsb(to));
        return moves;
    }

    // helper to generate promotion moves
    template <Color Us, Direction d>
    Move *makePromotions(Move *moves, U64 to)
    {
        while (to)
        {
            const Square s = popLsb(to);
            const Square from = s - relativeDir(Us, d);
            *moves++ = Move(from, s, PR_KNIGHT);
            *moves++ = Move(from, s, PR_BISHOP);
            *moves++ = Move(from, s, PR_ROOK);
            *moves++ = Move(from, s, PR_QUEEN);
        }
        return moves;
    }

    template <Color Us>
    Move *genCastlingMoves(const Board &board, Move *moves, const U64 occ)
    {
        int ply = board.getPly();

        // checks if king would be in check if it moved to the castling square
        U64 possible_checks = (occ | board.danger) & ooBlockersMask(Us);
        if (!possible_checks && board.history[ply].castle_rights.kingSide(Us))
            *moves++ = Us == WHITE ? Move(e1, g1, CASTLING) : Move(e8, g8, CASTLING);

        // ignoreLongCastlingDanger is used to get rid of the danger on the possibleChecks or b8 square
        possible_checks = ((occ | (board.danger & ~ignoreOOODanger(Us))) & oooBlockersMask(Us));
        if (!possible_checks && board.history[ply].castle_rights.queenSide(Us))
            *moves++ = Us == WHITE ? Move(e1, c1, CASTLING) : Move(e8, c8, CASTLING);

        return moves;
    }

    template <Color Us>
    Move *genPieceMoves(const Board &board, Move *moves, U64 occ, U64 cap_mask, U64 q_mask, const int num_checkers)
    {
        Square s;    // used to store square
        U64 attacks; // used to store piece attack squares;

        // pinned pieces cannot move if king is in check
        if (num_checkers == 0)
        {
            const Square our_king_sq = board.kingSq(Us);

            // for each pinned rook, bishop or queen
            U64 pinned_pieces = ~(~board.pinned | board.getPieceBB(Us, KNIGHT));
            while (pinned_pieces)
            {
                s = popLsb(pinned_pieces);
                // only include getAttacks that are aligned with our king
                attacks = getAttacks(typeOf(board.pieceAt(s)), s, occ) & LINE[our_king_sq][s];

                moves = make(moves, s, attacks & cap_mask);
                moves = make(moves, s, attacks & q_mask);
            }
        }

        // knight moves
        U64 our_knights = board.getPieceBB(Us, KNIGHT) & ~board.pinned;
        while (our_knights)
        {
            s = popLsb(our_knights);
            attacks = getAttacks(KNIGHT, s, occ);

            moves = make(moves, s, attacks & cap_mask);
            moves = make(moves, s, attacks & q_mask);
        }

        // bishops and queens
        U64 our_diag_sliders = board.diagSliders(Us) & ~board.pinned;
        while (our_diag_sliders)
        {
            s = popLsb(our_diag_sliders);
            attacks = getAttacks(BISHOP, s, occ);

            moves = make(moves, s, attacks & cap_mask);
            moves = make(moves, s, attacks & q_mask);
        }

        // rooks and queens
        U64 our_orth_sliders = board.orthSliders(Us) & ~board.pinned;
        while (our_orth_sliders)
        {
            s = popLsb(our_orth_sliders);
            attacks = getAttacks(ROOK, s, occ);

            moves = make(moves, s, attacks & cap_mask);
            moves = make(moves, s, attacks &q_mask);
        }

        return moves;
    }

    template <Color Us>
    Move *genPawnMoves(const Board &board, Move *moves, U64 occ, U64 cap_mask, U64 q_mask, const int num_checkers)
    {
        constexpr Color them = ~Us;
        const Square ep_sq = board.history[board.getPly()].ep_sq;
        const Square our_king_sq = board.kingSq(Us);

        // used to store square
        Square s;

        // check if moving pinned pawns releases a check
        if (num_checkers == 0)
        {
            // for each pinned pawn
            U64 pinned_pawns = board.pinned & board.getPieceBB(Us, PAWN);
            while (pinned_pawns)
            {
                s = popLsb(pinned_pawns);

                if (rankOf(s) == relativeRank(Us, RANK_7))
                {
                    U64 attacks = pawnAttacks(Us, s) & cap_mask & LINE[our_king_sq][s];
                    // quiet promotions are impossible since it would leave the king in check
                    while (attacks)
                    {
                        const Square to = popLsb(attacks);
                        *moves++ = Move(s, to, PR_KNIGHT);
                        *moves++ = Move(s, to, PR_BISHOP);
                        *moves++ = Move(s, to, PR_ROOK);
                        *moves++ = Move(s, to, PR_QUEEN);
                    }
                }
                else
                {
                    const U64 attacks = pawnAttacks(Us, s) & cap_mask & LINE[s][our_king_sq];
                    moves = make(moves, s, attacks);

                    const U64 single_push = shift(relativeDir(Us, NORTH), SQUARE_BB[s]) & ~occ & LINE[our_king_sq][s];
                    moves = make(moves, s, single_push);

                    const U64 double_push = shift(relativeDir(Us, NORTH), single_push & MASK_RANK[relativeRank(Us, RANK_3)]);
                    moves = make(moves, s, double_push & ~occ & LINE[our_king_sq][s]);
                }
            }

            // e.p. moves
            if (ep_sq != NO_SQUARE)
            {
                const U64 their_orth_sliders = board.orthSliders(them);
                const U64 ep_capture_bb = pawnAttacks(them, ep_sq) & board.getPieceBB(Us, PAWN);
                U64 can_capture = ep_capture_bb & ~board.pinned;

                while (can_capture)
                {
                    s = popLsb(can_capture);
                    // pseudo-pinned e.p. case
                    const U64 new_occ_mask = occ ^ SQUARE_BB[s] ^ shift(relativeDir(Us, SOUTH), SQUARE_BB[ep_sq]);
                    U64 attacker = slidingAttacks(our_king_sq, new_occ_mask, MASK_RANK[rankOf(our_king_sq)]);

                    if ((attacker & their_orth_sliders) == 0)
                        *moves++ = Move(s, ep_sq, EN_PASSANT);
                }

                // pinned pawns can only capture e.p. if they are pinned diagonally and the e.p. square is in line with the king
                can_capture = ep_capture_bb & board.pinned & LINE[ep_sq][our_king_sq];
                if (can_capture)
                    *moves++ = Move(bsf(can_capture), ep_sq, EN_PASSANT);
            }
        }

        // contains non-pinned pawns which are not on the last rank
        U64 our_pawns = board.getPieceBB(Us, PAWN) & ~board.pinned & ~MASK_RANK[relativeRank(Us, RANK_7)];
        // single pawn pushes
        U64 single_push = shift(relativeDir(Us, NORTH), our_pawns) & ~occ;
        // double pawn pushes (only the ones that are on rank 3/6 are considered)
        U64 double_push = shift(relativeDir(Us, NORTH), single_push & MASK_RANK[relativeRank(Us, RANK_3)]) & q_mask;
        // quiet mask is applied later, to consider the possibility of a double push blocking a check
        single_push &= q_mask;

        while (single_push)
        {
            s = popLsb(single_push);
            *moves++ = Move(s - relativeDir(Us, NORTH), s);
        }

        while (double_push)
        {
            s = popLsb(double_push);
            *moves++ = Move(s - relativeDir(Us, NORTH_NORTH), s);
        }

        // captures
        U64 left_captures = shift(relativeDir(Us, NORTH_WEST), our_pawns) & cap_mask;
        while (left_captures)
        {
            s = popLsb(left_captures);
            *moves++ = Move(s - relativeDir(Us, NORTH_WEST), s);
        }

        U64 right_captures = shift(relativeDir(Us, NORTH_EAST), our_pawns) & cap_mask;
        while (right_captures)
        {
            s = popLsb(right_captures);
            *moves++ = Move(s - relativeDir(Us, NORTH_EAST), s);
        }

        // promotions
        our_pawns = board.getPieceBB(Us, PAWN) & ~board.pinned & MASK_RANK[relativeRank(Us, RANK_7)];
        if (our_pawns)
        {
            // attacks contains squares that the pawns can move to
            U64 attacks;

            // quiet promotions
            attacks = shift(relativeDir(Us, NORTH), our_pawns) & q_mask;
            moves = makePromotions<Us, NORTH>(moves, attacks);

            // promotion captures
            attacks = shift(relativeDir(Us, NORTH_WEST), our_pawns) & cap_mask;
            moves = makePromotions<Us, NORTH_WEST>(moves, attacks);

            attacks = shift(relativeDir(Us, NORTH_EAST), our_pawns) & cap_mask;
            moves = makePromotions<Us, NORTH_EAST>(moves, attacks);
        }

        return moves;
    }

    template <Color Us>
    Move *genLegalMoves(Board &board, Move *moves)
    {
        const Color them = ~Us;
        const Square ep_sq = board.history[board.getPly()].ep_sq;
        const Square our_king_sq = board.kingSq(Us);
        const U64 our_occ = board.occupancy(Us);
        const U64 their_occ = board.occupancy(them);
        const U64 occ = our_occ | their_occ;

        board.initThreats();
        board.initCheckersAndPinned();

        // generate king moves
        const U64 attacks = getAttacks(KING, our_king_sq, occ) & ~(our_occ | board.danger);

        moves = make(moves, our_king_sq, attacks & their_occ);
        moves = make(moves, our_king_sq, attacks & ~their_occ);

        // if double check, then only king moves are legal
        const int num_checkers = sparsePopCount(board.checkers);
        if (num_checkers == 2)
            return moves;

        U64 cap_mask, q_mask;

        // single check
        if (num_checkers == 1)
        {
            const Square checker_sq = bsf(board.checkers);
            const Piece checker_piece = board.pieceAt(checker_sq);

            // holds our pieces that can capture the checking piece
            U64 can_capture;
            if (checker_piece == makePiece(them, PAWN))
            {
                // if the checker is a pawn, we must check for e.p. moves that can capture it
                if (board.checkers == shift(relativeDir(Us, SOUTH), SQUARE_BB[ep_sq]))
                {
                    const U64 our_pawns = board.getPieceBB(Us, PAWN);

                    can_capture = pawnAttacks(them, ep_sq) & our_pawns & ~board.pinned;
                    while (can_capture)
                        *moves++ = Move(popLsb(can_capture), ep_sq, EN_PASSANT);
                }
            }

            // if checker is either a pawn or a knight, the only legal moves are to capture it
            if (checker_piece == makePiece(them, KNIGHT))
            {
                can_capture = board.attackersTo(Us, checker_sq, occ) & ~board.pinned;
                while (can_capture)
                    *moves++ = Move(popLsb(can_capture), checker_sq);
                return moves;
            }

            // we must capture the checking piece
            cap_mask = board.checkers;
            // or we block it
            q_mask = SQUARES_BETWEEN[our_king_sq][checker_sq];
        }
        else
        {
            // we can capture any enemy piece
            cap_mask = their_occ;
            // we can move to any square which is not occupied
            q_mask = ~occ;

            moves = genCastlingMoves<Us>(board, moves, occ);
        }

        moves = genPieceMoves<Us>(board, moves, occ, cap_mask, q_mask, num_checkers);
        moves = genPawnMoves<Us>(board, moves, occ, cap_mask, q_mask, num_checkers);

        return moves;
    }

    class MoveList
    {
    public:
        MoveList(Board &board)
        {
            last = board.getTurn() == WHITE ? genLegalMoves<WHITE>(board, list) : genLegalMoves<BLACK>(board, list);
        }

        constexpr Move &operator[](int i)
        {
            assert(i >= 0 && i < size());
            return list[i];
        }

        int find(Move m) const
        {
            for (int i = 0; i < size(); i++)
                if (list[i] == m)
                    return i;
            return -1;
        }

        const Move *begin() const { return list; }
        const Move *end() const { return last; }
        int size() const { return last - list; }

    private:
        Move list[MAX_MOVES];
        Move *last;
    };

} // namespace Chess

#endif // MOVEGEN_H
