#pragma once

#include "board.h"
#include <type_traits> // std::is_same

#include "board.h"

namespace Chess {

enum GenType : int { NOISY = 0, QUIETS = 1, QUIET_CHECKERS = 2, LEGALS = 3 };

template <GenType gt, Direction d, MoveType mt> Move *makePromotions(Move *ml, Square to) {
    assert(to >= a1 && to <= h8);
    assert(!(to >= a2 && to <= h7));

    if constexpr(gt != QUIETS)
        *ml++ = Move(to - d, to, mt);

    if constexpr(gt != NOISY) {
        *ml++ = Move(to - d, to, MoveType(mt - 1));
        *ml++ = Move(to - d, to, MoveType(mt - 2));
        *ml++ = Move(to - d, to, MoveType(mt - 3));
    }

    return ml;
}

template <Color us, GenType gt> Move *genPawnMoves(const Board &board, Move *ml, const U64 targets) {
    constexpr Color them = ~us;
    constexpr U64 rank7_bb = MASK_RANK[relRank(us, RANK_7)];
    constexpr Direction up = us == WHITE ? NORTH : SOUTH;
    constexpr Direction up_right = (us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction up_left = (us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const U64 them_bb = board.occupancy(them);
    const U64 occ = board.occupancy(us) | them_bb;
    const U64 empty_sqs = ~occ;

    const U64 pawns = board.getPieceBB(us, PAWN);
    const U64 pawns_non7 = pawns & ~rank7_bb;

    // single and double pawn pushes, no promotions
    if constexpr(gt != NOISY) {
        U64 b1 = shift(up, pawns_non7) & empty_sqs;
        U64 b2 = shift(up, b1 & MASK_RANK[relRank(us, RANK_3)]) & empty_sqs;

        b1 &= targets;
        b2 &= targets;

        while(b1) {
            Square to = popLsb(b1);
            *ml++ = Move(to - up, to, QUIET);
        }

        while(b2) {
            Square to = popLsb(b2);
            *ml++ = Move(to - up - up, to, QUIET);
        }
    }

    // standard and en passant captures, no promotions
    if constexpr(gt != QUIETS) {
        U64 b1 = shift(up_right, pawns_non7) & them_bb & targets;
        U64 b2 = shift(up_left, pawns_non7) & them_bb & targets;

        while(b1) {
            Square to = popLsb(b1);
            *ml++ = Move(to - up_right, to, CAPTURE);
        }

        while(b2) {
            Square to = popLsb(b2);
            *ml++ = Move(to - up_left, to, CAPTURE);
        }

        int ply = board.getPly();
        Square ep_sq = board.history[ply].ep_sq;

        if(ep_sq != NO_SQUARE) {
            assert(rankOf(ep_sq) == relRank(us, RANK_6));

            b1 = pawns_non7 & getPawnAttacks(them, ep_sq);
            while(b1)
                *ml++ = Move(popLsb(b1), ep_sq, EN_PASSANT);
        }
    }

    // promotions, both capture and non-capture
    const U64 pawns_on7 = pawns & rank7_bb;
    U64 b1 = shift(up_right, pawns_on7) & them_bb & targets;
    U64 b2 = shift(up_left, pawns_on7) & them_bb & targets;
    U64 b3 = shift(up, pawns_on7) & empty_sqs & targets;

    if constexpr(gt != QUIETS) {
        while(b1)
            ml = makePromotions<gt, up_right, PC_QUEEN>(ml, popLsb(b1));
        while(b2)
            ml = makePromotions<gt, up_left, PC_QUEEN>(ml, popLsb(b2));
    }

    while(b3)
        ml = makePromotions<gt, up, PQ_QUEEN>(ml, popLsb(b3));

    return ml;
}

template <Color us, PieceType pt, GenType gt> Move *genPieceMoves(const Board &board, Move *ml, const U64 targets) {
    const U64 them_bb = board.occupancy(~us);
    const U64 occ = board.occupancy(us) | them_bb;

    U64 piece = board.getPieceBB(us, pt);
    while(piece) {
        Square from = popLsb(piece);
        U64 attacks = getAttacks(pt, from, occ) & targets;

        if constexpr(gt != NOISY) {
            U64 target = attacks & ~occ;
            while(target)
                *ml++ = Move(from, popLsb(target), QUIET);
        }

        if constexpr(gt != QUIETS) {
            U64 target = attacks & them_bb;
            while(target)
                *ml++ = Move(from, popLsb(target), CAPTURE);
        }
    }

    return ml;
}

template <Color us, GenType gt> Move *genAll(const Board &board, Move *ml) {
    const int ply = board.getPly();
    const StateInfo &info = board.history[ply];
    const Square ksq = board.kingSq(us);

    const U64 us_bb = board.occupancy(us);
    const U64 them_bb = board.occupancy(~us);
    const U64 occ = us_bb | them_bb;

    const U64 checkers = info.checkers;
    const U64 danger = info.danger;

    // if double check, then only king moves are legal
    if(popCount(checkers) > 1)
        return genPieceMoves<us, KING, gt>(board, ml, ~danger);

    const U64 targets = checkers ? SQUARES_BETWEEN[ksq][lsb(checkers)] | checkers : -1ULL;

    ml = genPawnMoves<us, gt>(board, ml, targets);
    ml = genPieceMoves<us, KNIGHT, gt>(board, ml, targets);
    ml = genPieceMoves<us, BISHOP, gt>(board, ml, targets);
    ml = genPieceMoves<us, ROOK, gt>(board, ml, targets);
    ml = genPieceMoves<us, QUEEN, gt>(board, ml, targets);
    ml = genPieceMoves<us, KING, gt>(board, ml, ~danger);

    // castling moves
    if(gt != NOISY && !checkers) {
        U64 not_free = (occ | danger) & OO_BLOCKERS_MASK[us];
        if(!not_free && info.castle_rights.kingSide(us))
            *ml++ = us == WHITE ? Move(e1, g1, CASTLING) : Move(e8, g8, CASTLING);

        // ignore the square b1/b8 since the king does move there
        not_free = (occ | (danger & ~SQUARE_BB[relSquare(us, b1)])) & OOO_BLOCKERS_MASK[us];
        if(!not_free && info.castle_rights.queenSide(us))
            *ml++ = us == WHITE ? Move(e1, c1, CASTLING) : Move(e8, c8, CASTLING);
    }

    return ml;
}

template <Color us> Move *genQuietCheckers(const Board &board, Move *ml) {
    constexpr Color them = ~us;
    const Square opp_ksq = board.kingSq(them);
    const U64 occ = board.occupancy();
    const U64 bishop_attacks = getBishopAttacks(opp_ksq, occ);
    const U64 rook_attacks = getRookAttacks(opp_ksq, occ);

    ml = genPawnMoves<us, QUIETS>(board, ml, getPawnAttacks(them, opp_ksq));
    ml = genPieceMoves<us, KNIGHT, QUIETS>(board, ml, getKnightAttacks(opp_ksq));
    ml = genPieceMoves<us, BISHOP, QUIETS>(board, ml, bishop_attacks);
    ml = genPieceMoves<us, ROOK, QUIETS>(board, ml, rook_attacks);
    ml = genPieceMoves<us, QUEEN, QUIETS>(board, ml, bishop_attacks | rook_attacks);

    return ml;
}

inline Move *genLegals(const Board &board, Move *ml) {
    const Color us = board.getTurn();
    const U64 pinned = board.history[board.getPly()].pinned;

    Move *curr = ml;

    ml = us == WHITE ? genAll<WHITE, LEGALS>(board, ml) : genAll<BLACK, LEGALS>(board, ml);
    while(curr != ml)
        if(((pinned & SQUARE_BB[curr->from()]) || curr->type() == EN_PASSANT) && !board.isLegal(*curr))
            *curr = *(--ml);
        else
            ++curr;

    return ml;
}

template <typename Type = Move> class MoveList {
  private:
    Type list[MAX_MOVES];
    Type *last;

  public:
    MoveList() : last(list) {}

    constexpr Type &operator[](int i) {
        return list[i];
    }

    template <GenType gt> void gen(const Board &board) {
        static_assert(std::is_same<Type, Move>::value, "MoveList type must be Move to generate moves.");
        last = list; // reset the list

        if(gt == LEGALS)
            last = genLegals(board, list);
        else if(gt == QUIET_CHECKERS)
            last = board.getTurn() == WHITE ? genQuietCheckers<WHITE>(board, list) : genQuietCheckers<BLACK>(board, list);
        else
            last = board.getTurn() == WHITE ? genAll<WHITE, gt>(board, list) : genAll<BLACK, gt>(board, list);
    }

    void add(Type m) {
        assert(last < list + MAX_MOVES);
        *last++ = m;
    }

    const Type *begin() const {
        return list;
    }

    const Type *end() const {
        return last;
    }

    int size() const {
        return last - list;
    }
};

} // namespace Chess
