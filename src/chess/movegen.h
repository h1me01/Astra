#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "board.h"

namespace Chess
{
    enum MoveType
    {
        CAPTURE_MOVES, ALL_MOVES
    };

    constexpr U64 ooMask(Color c) { return c == WHITE ? WHITE_OO_MASK : BLACK_OO_MASK; }
    constexpr U64 oooMask(Color c) { return c == WHITE ? WHITE_OOO_MASK : BLACK_OOO_MASK; }

    constexpr U64 ooBlockersMask(Color c) { return c == WHITE ? 0x60 : 0x6000000000000000; }
    constexpr U64 oooBlockersMask(Color c) { return c == WHITE ? 0xe : 0xE00000000000000; }

    constexpr U64 ignoreOOODanger(Color c) { return c == WHITE ? 0x2 : 0x200000000000000; }

    // helper to generate quiet moves
    template <MoveFlags MF>
    Move* make(Move* moves, Square from, U64 to)
    {
        while (to)
            *moves++ = Move(from, popLsb(to), MF);
        return moves;
    }

    // helper to generate promotion capture moves
    template <Color Us, Direction d, MoveFlags mf>
    Move* makePromotions(Move* moves, U64 to)
    {
        while (to)
        {
            const Square s = popLsb(to);
            const Square from = s - relativeDir(Us, d);
            *moves++ = Move(from, s, mf);
            *moves++ = Move(from, s, MoveFlags(mf + 1));
            *moves++ = Move(from, s, MoveFlags(mf + 2));
            *moves++ = Move(from, s, MoveFlags(mf + 3));
        }
        return moves;
    }

    template <Color Us>
    U64 diagPawnAttacks(const U64 pawns)
    {
        return Us == WHITE ? shift(NORTH_WEST, pawns) | shift(NORTH_EAST, pawns) : shift(SOUTH_WEST, pawns) | shift(SOUTH_EAST, pawns);
    }

    template <Color Us>
    void initDangerMask(Board& board, U64 occ)
    {
        constexpr Color them = ~Us;

        // enemy king attacks
        board.danger = getAttacks(KING, board.kingSq(them), occ);
        // enemy pawns attacks
        board.danger |= diagPawnAttacks<them>(board.getPieceBB(them, PAWN));

        // enemy knights attacks
        U64 their_knights = board.getPieceBB(them, KNIGHT);
        while (their_knights)
            board.danger |= getAttacks(KNIGHT, popLsb(their_knights), occ);

        // exclude our king from the occupancy, because checks should not be included
        occ ^= SQUARE_BB[board.kingSq(Us)];

        // enemy bishop and queen attacks
        U64 their_diag_sliders = board.diagSliders(them);
        while (their_diag_sliders)
            board.danger |= getAttacks(BISHOP, popLsb(their_diag_sliders), occ);

        // enemy rook and queen attacks
        U64 their_orth_sliders = board.orthSliders(them);
        while (their_orth_sliders)
            board.danger |= getAttacks(ROOK, popLsb(their_orth_sliders), occ);
    }

    template <Color Us>
    void initCheckerMask(Board& board, const Square king_sq)
    {
        constexpr Color them = ~Us;
        const U64 their_occ = board.occupancy(them);
        const U64 our_occ = board.occupancy(Us);

        // enemy pawns attacks at our king
        board.checkers = pawnAttacks(Us, king_sq) & board.getPieceBB(them, PAWN);;
        // enemy knights attacks at our king
        board.checkers |= getAttacks(KNIGHT, king_sq, our_occ | their_occ) & board.getPieceBB(them, KNIGHT);

        // potential enemy bishop, rook and queen attacks at our king
        U64 candidates = (getAttacks(ROOK, king_sq, their_occ) & board.orthSliders(them))
            | (getAttacks(BISHOP, king_sq, their_occ) & board.diagSliders(them));

        board.pinned = 0;
        while (candidates)
        {
            const Square s = popLsb(candidates);
            U64 blockers = SQUARES_BETWEEN[king_sq][s] & our_occ;
            // if between the enemy slider attack and our king is no of our pieces
            // add the enemy piece to the checkers bitboard
            if (blockers == 0)
                board.checkers ^= SQUARE_BB[s];
            else if ((blockers & (blockers - 1)) == 0)
                // if there is only one of our piece between them, add our piece to the pinned
                board.pinned ^= blockers;
        }
    }

    template <Color Us>
    Move* genCastlingMoves(const Board& board, Move* moves, const U64 occ)
    {
        const U64 castleMask = board.history[board.getPly()].castle_mask;

        // checks if king would be in check if it moved to the castling square
        U64 possible_checks = (occ | board.danger) & ooBlockersMask(Us);
        // checks if king and the h-rook have moved
        U64 is_allowed = castleMask & ooMask(Us);

        if (!(possible_checks | is_allowed))
            *moves++ = Us == WHITE ? Move(e1, g1, OO) : Move(e8, g8, OO);

        // ignoreLongCastlingDanger is used to get rid of the danger on the possibleChecks or b8 square
        possible_checks = ((occ | (board.danger & ~ignoreOOODanger(Us))) & oooBlockersMask(Us));

        is_allowed = castleMask & oooMask(Us);

        if (!(possible_checks | is_allowed))
            *moves++ = Us == WHITE ? Move(e1, c1, OOO) : Move(e8, c8, OOO);

        return moves;
    }

    template <Color Us, MoveType mt>
    Move* genPieceMoves(const Board& board, Move* moves, const U64 occ, const int num_checkers)
    {
        Square s; // used to store square
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

                moves = make<CAPTURE>(moves, s, attacks & board.capture_mask);
                if (mt == ALL_MOVES)
                    moves = make<QUIET>(moves, s, attacks & board.quiet_mask);
            }
        }

        // knight moves
        U64 our_knights = board.getPieceBB(Us, KNIGHT) & ~board.pinned;
        while (our_knights)
        {
            s = popLsb(our_knights);
            attacks = getAttacks(KNIGHT, s, occ);

            moves = make<CAPTURE>(moves, s, attacks & board.capture_mask);
            if (mt == ALL_MOVES)
                moves = make<QUIET>(moves, s, attacks & board.quiet_mask);
        }

        // bishops and queens
        U64 our_diag_sliders = board.diagSliders(Us) & ~board.pinned;
        while (our_diag_sliders)
        {
            s = popLsb(our_diag_sliders);
            attacks = getAttacks(BISHOP, s, occ);

            moves = make<CAPTURE>(moves, s, attacks & board.capture_mask);
            if (mt == ALL_MOVES)
                moves = make<QUIET>(moves, s, attacks & board.quiet_mask);
        }

        // rooks and queens
        U64 our_orth_sliders = board.orthSliders(Us) & ~board.pinned;
        while (our_orth_sliders)
        {
            s = popLsb(our_orth_sliders);
            attacks = getAttacks(ROOK, s, occ);

            moves = make<CAPTURE>(moves, s, attacks & board.capture_mask);
            if (mt == ALL_MOVES)
                moves = make<QUIET>(moves, s, attacks & board.quiet_mask);
        }

        return moves;
    }

    template <Color Us, MoveType mt>
    Move* genPawnMoves(const Board& board, Move* moves, const U64 occ, const int num_checkers)
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
                    U64 attacks = pawnAttacks(Us, s) & board.capture_mask & LINE[our_king_sq][s];
                    // quiet promotions are impossible since it would leave the king in check
                    while (attacks)
                    {
                        const Square to = popLsb(attacks);
                        *moves++ = Move(s, to, PC_KNIGHT);
                        *moves++ = Move(s, to, PC_BISHOP);
                        *moves++ = Move(s, to, PC_ROOK);
                        *moves++ = Move(s, to, PC_QUEEN);
                    }
                }
                else
                {
                    const U64 attacks = pawnAttacks(Us, s) & board.capture_mask & LINE[s][our_king_sq];
                    moves = make<CAPTURE>(moves, s, attacks);

                    if (mt == ALL_MOVES)
                    {
                        const U64 single_push = shift(relativeDir(Us, NORTH), SQUARE_BB[s]) & ~occ & LINE[our_king_sq][s];
                        moves = make<QUIET>(moves, s, single_push);

                        const U64 double_push = shift(relativeDir(Us, NORTH), single_push & MASK_RANK[relativeRank(Us, RANK_3)]);
                        moves = make<DOUBLE_PUSH>(moves, s, double_push & ~occ & LINE[our_king_sq][s]);
                    }
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
        U64 double_push = shift(relativeDir(Us, NORTH), single_push & MASK_RANK[relativeRank(Us, RANK_3)]) & board.quiet_mask;
        // quiet mask is applied later, to consider the possibility of a double push blocking a check
        single_push &= board.quiet_mask;

        while (single_push && mt == ALL_MOVES)
        {
            s = popLsb(single_push);
            *moves++ = Move(s - relativeDir(Us, NORTH), s, QUIET);
        }

        while (double_push && mt == ALL_MOVES)
        {
            s = popLsb(double_push);
            *moves++ = Move(s - relativeDir(Us, NORTH_NORTH), s, DOUBLE_PUSH);
        }

        // captures
        U64 left_captures = shift(relativeDir(Us, NORTH_WEST), our_pawns) & board.capture_mask;
        while (left_captures)
        {
            s = popLsb(left_captures);
            *moves++ = Move(s - relativeDir(Us, NORTH_WEST), s, CAPTURE);
        }

        U64 right_captures = shift(relativeDir(Us, NORTH_EAST), our_pawns) & board.capture_mask;
        while (right_captures)
        {
            s = popLsb(right_captures);
            *moves++ = Move(s - relativeDir(Us, NORTH_EAST), s, CAPTURE);
        }

        // promotions
        our_pawns = board.getPieceBB(Us, PAWN) & ~board.pinned & MASK_RANK[relativeRank(Us, RANK_7)];
        if (our_pawns)
        {
            // attacks contains squares that the pawns can move to
            U64 attacks;

            // quiet promotions
            if (mt == ALL_MOVES)
            {
                attacks = shift(relativeDir(Us, NORTH), our_pawns) & board.quiet_mask;
                moves = makePromotions<Us, NORTH, PR_KNIGHT>(moves, attacks);
            }

            // promotion captures
            attacks = shift(relativeDir(Us, NORTH_WEST), our_pawns) & board.capture_mask;
            moves = makePromotions<Us, NORTH_WEST, PC_KNIGHT>(moves, attacks);

            attacks = shift(relativeDir(Us, NORTH_EAST), our_pawns) & board.capture_mask;
            moves = makePromotions<Us, NORTH_EAST, PC_KNIGHT>(moves, attacks);
        }

        return moves;
    }

    template <Color Us, MoveType mt>
    Move* genLegalMoves(Board& board, Move* moves)
    {
        const Color them = ~Us;
        const Square ep_sq = board.history[board.getPly()].ep_sq;
        const Square our_king_sq = board.kingSq(Us);
        const U64 our_occ = board.occupancy(Us);
        const U64 their_occ = board.occupancy(them);
        const U64 occ = our_occ | their_occ;

        initDangerMask<Us>(board, occ);
        initCheckerMask<Us>(board, our_king_sq);

        // generate king moves
        const U64 attacks = getAttacks(KING, our_king_sq, occ) & ~(our_occ | board.danger);

        moves = make<CAPTURE>(moves, our_king_sq, attacks & their_occ);
        if (mt == ALL_MOVES)
            moves = make<QUIET>(moves, our_king_sq, attacks & ~their_occ);

        // if double check, then only king moves are legal
        const int num_checkers = sparsePopCount(board.checkers);
        if (num_checkers == 2)
            return moves;

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
                can_capture = board.attackers(Us, checker_sq, occ) & ~board.pinned;
                while (can_capture)
                    *moves++ = Move(popLsb(can_capture), checker_sq, CAPTURE);
                return moves;
            }

            // we must capture the checking piece
            board.capture_mask = board.checkers;
            // or we block it
            board.quiet_mask = SQUARES_BETWEEN[our_king_sq][checker_sq];
        }
        else
        {
            // we can capture any enemy piece
            board.capture_mask = their_occ;
            // we can move to any square which is not occupied
            board.quiet_mask = ~occ;

            if (mt == ALL_MOVES)
                moves = genCastlingMoves<Us>(board, moves, occ);
        }

        moves = genPieceMoves<Us, mt>(board, moves, occ, num_checkers);
        moves = genPawnMoves<Us, mt>(board, moves, occ, num_checkers);

        return moves;
    }

    class MoveList
    {
    public:
        MoveList(Board& board, MoveType mt = ALL_MOVES)
        {
            bool is_w = board.getTurn() == WHITE;

            if (mt == CAPTURE_MOVES)
                last = is_w ? genLegalMoves<WHITE, CAPTURE_MOVES>(board, list) : genLegalMoves<BLACK, CAPTURE_MOVES>(board, list);
            else
                last = is_w ? genLegalMoves<WHITE, ALL_MOVES>(board, list) : genLegalMoves<BLACK, ALL_MOVES>(board, list);
        }

        constexpr Move& operator[](int i)
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

        const Move* begin() const { return list; }
        const Move* end() const { return last; }
        int size() const { return last - list; }

    private:
        Move list[MAX_MOVES];
        Move* last;
    };

} // namespace Chess

#endif //MOVEGEN_H
