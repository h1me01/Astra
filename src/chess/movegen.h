#pragma once

#include "board.h"

namespace Chess {

enum GenType : int { //
    NOISY = 0,
    QUIETS = 1,
    QUIET_CHECKS = 2,
    LEGALS = 3
};

template <GenType gt, Direction d, MoveType mt> //
Move *make_promotions(Move *ml, Square to) {
    assert(valid_sq(to));
    assert(!(to >= a2 && to <= h7));

    *ml++ = Move(to - d, to, mt);
    *ml++ = Move(to - d, to, MoveType(mt - 1));
    *ml++ = Move(to - d, to, MoveType(mt - 2));
    *ml++ = Move(to - d, to, MoveType(mt - 3));

    return ml;
}

template <Color us, GenType gt> //
Move *gen_pawnmoves(const Board &board, Move *ml, const U64 targets) {
    constexpr Color them = ~us;
    constexpr U64 rank7_bb = rank_mask(rel_rank(us, RANK_7));
    constexpr Direction up = us == WHITE ? NORTH : SOUTH;
    constexpr Direction up_right = (us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction up_left = (us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const U64 them_bb = board.get_occupancy(them);
    const U64 occ = board.get_occupancy(us) | them_bb;
    const U64 empty_sqs = ~occ;

    const U64 pawns = board.get_piecebb(us, PAWN);
    const U64 pawns_non7 = pawns & ~rank7_bb;

    // single and double pawn pushes, no promotions
    if constexpr(gt != NOISY) {
        U64 b1 = shift<up>(pawns_non7) & empty_sqs;
        U64 b2 = shift<up>(b1 & rank_mask(rel_rank(us, RANK_3))) & empty_sqs;

        b1 &= targets;
        b2 &= targets;

        while(b1) {
            Square to = pop_lsb(b1);
            *ml++ = Move(to - up, to, QUIET);
        }

        while(b2) {
            Square to = pop_lsb(b2);
            *ml++ = Move(to - up - up, to, QUIET);
        }
    }

    if constexpr(gt != QUIETS) {
        // standard captures
        U64 b1 = shift<up_right>(pawns_non7) & them_bb & targets;
        U64 b2 = shift<up_left>(pawns_non7) & them_bb & targets;

        while(b1) {
            Square to = pop_lsb(b1);
            *ml++ = Move(to - up_right, to, CAPTURE);
        }

        while(b2) {
            Square to = pop_lsb(b2);
            *ml++ = Move(to - up_left, to, CAPTURE);
        }

        // en passant
        Square ep_sq = board.get_state().ep_sq;
        if(valid_sq(ep_sq)) {
            assert(sq_rank(ep_sq) == rel_rank(us, RANK_6));

            b1 = pawns_non7 & get_pawn_attacks(them, ep_sq);
            while(b1)
                *ml++ = Move(pop_lsb(b1), ep_sq, EN_PASSANT);
        }

        // promotions
        const U64 pawns_on7 = pawns & rank7_bb;
        U64 pb1 = shift<up_right>(pawns_on7) & them_bb & targets;
        U64 pb2 = shift<up_left>(pawns_on7) & them_bb & targets;
        U64 pb3 = shift<up>(pawns_on7) & empty_sqs & targets;

        while(pb1)
            ml = make_promotions<gt, up_right, PC_QUEEN>(ml, pop_lsb(pb1));
        while(pb2)
            ml = make_promotions<gt, up_left, PC_QUEEN>(ml, pop_lsb(pb2));
        while(pb3)
            ml = make_promotions<gt, up, PQ_QUEEN>(ml, pop_lsb(pb3));
    }

    return ml;
}

template <Color us, PieceType pt, GenType gt> //
Move *gen_piecemoves(const Board &board, Move *ml, const U64 targets) {
    const U64 them_bb = board.get_occupancy(~us);
    const U64 occ = board.get_occupancy(us) | them_bb;

    U64 piece = board.get_piecebb(us, pt);
    while(piece) {
        Square from = pop_lsb(piece);
        U64 attacks = get_attacks(pt, from, occ) & targets;

        if constexpr(gt != NOISY) {
            U64 target = attacks & ~occ;
            while(target)
                *ml++ = Move(from, pop_lsb(target), QUIET);
        }

        if constexpr(gt != QUIETS) {
            U64 target = attacks & them_bb;
            while(target)
                *ml++ = Move(from, pop_lsb(target), CAPTURE);
        }
    }

    return ml;
}

template <Color us, GenType gt> //
Move *gen_all(const Board &board, Move *ml) {
    const StateInfo &info = board.get_state();
    const Square ksq = board.king_sq(us);

    const U64 us_bb = board.get_occupancy(us);
    const U64 them_bb = board.get_occupancy(~us);
    const U64 occ = us_bb | them_bb;

    const U64 checkers = info.checkers;
    const U64 danger = info.danger;

    // if double check, then only king moves are legal
    if(pop_count(checkers) > 1)
        return gen_piecemoves<us, KING, gt>(board, ml, ~danger);

    const U64 targets = checkers ? between_bb(ksq, lsb(checkers)) | checkers : -1ULL;

    ml = gen_pawnmoves<us, gt>(board, ml, targets);
    ml = gen_piecemoves<us, KNIGHT, gt>(board, ml, targets);
    ml = gen_piecemoves<us, BISHOP, gt>(board, ml, targets);
    ml = gen_piecemoves<us, ROOK, gt>(board, ml, targets);
    ml = gen_piecemoves<us, QUEEN, gt>(board, ml, targets);
    ml = gen_piecemoves<us, KING, gt>(board, ml, ~danger);

    // castling moves
    if(gt != NOISY && !checkers) {
        U64 not_free = (occ | danger) & OO_BLOCKERS_MASK[us];
        if(!not_free && info.castle_rights.kingside(us))
            *ml++ = us == WHITE ? Move(e1, g1, CASTLING) : Move(e8, g8, CASTLING);

        // ignore the square b1/b8 since the king does move there
        not_free = (occ | (danger & ~square_bb(rel_sq(us, b1)))) & OOO_BLOCKERS_MASK[us];
        if(!not_free && info.castle_rights.queenside(us))
            *ml++ = us == WHITE ? Move(e1, c1, CASTLING) : Move(e8, c8, CASTLING);
    }

    return ml;
}

template <Color us> //
Move *gen_quiet_checkers(const Board &board, Move *ml) {
    constexpr Color them = ~us;
    const Square opp_ksq = board.king_sq(them);
    const U64 occ = board.get_occupancy();
    const U64 bishop_attacks = get_attacks(BISHOP, opp_ksq, occ);
    const U64 rook_attacks = get_attacks(ROOK, opp_ksq, occ);

    ml = gen_pawnmoves<us, QUIETS>(board, ml, get_pawn_attacks(them, opp_ksq));
    ml = gen_piecemoves<us, KNIGHT, QUIETS>(board, ml, get_attacks(KNIGHT, opp_ksq));
    ml = gen_piecemoves<us, BISHOP, QUIETS>(board, ml, bishop_attacks);
    ml = gen_piecemoves<us, ROOK, QUIETS>(board, ml, rook_attacks);
    ml = gen_piecemoves<us, QUEEN, QUIETS>(board, ml, bishop_attacks | rook_attacks);

    return ml;
}

inline Move *gen_legals(const Board &board, Move *ml) {
    const Color us = board.get_stm();
    const Square ksq = board.king_sq(us);
    const U64 pinned = board.get_state().blockers[us] & board.get_occupancy(us);

    Move *curr = ml;

    ml = (us == WHITE) ? gen_all<WHITE, LEGALS>(board, ml) //
                       : gen_all<BLACK, LEGALS>(board, ml);

    while(curr != ml) {
        if(((pinned & square_bb(curr->from())) || curr->is_ep() || curr->from() == ksq) && !board.is_legal(*curr)) {
            *curr = *(--ml);
        } else {
            ++curr;
        }
    }

    return ml;
}

template <typename Type = Move> //
class MoveList {
  public:
    MoveList() : last(list) {}

    constexpr Type &operator[](int i) {
        return list[i];
    }

    constexpr const Type &operator[](int i) const {
        return list[i];
    }

    template <GenType gt> //
    void gen(const Board &board) {
        last = list; // reset the list

        auto gen_moves = [&](Move *move_list) -> Move * {
            if(gt == LEGALS)
                return gen_legals(board, move_list);
            else if(gt == QUIET_CHECKS)
                return board.get_stm() == WHITE ? gen_quiet_checkers<WHITE>(board, move_list)
                                                : gen_quiet_checkers<BLACK>(board, move_list);
            else
                return board.get_stm() == WHITE ? gen_all<WHITE, gt>(board, move_list)
                                                : gen_all<BLACK, gt>(board, move_list);
        };

        if constexpr(std::is_same_v<Type, Move>) {
            last = gen_moves(list);
        } else {
            Move temp_list[MAX_MOVES];
            Move *temp_last = gen_moves(temp_list);

            for(Move *it = temp_list; it != temp_last; ++it)
                *last++ = Type(*it);
        }
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

    int idx_of(const Type &move) const {
        for(int i = 0; i < size(); i++)
            if(list[i] == move)
                return i;
        return -1;
    }

    int size() const {
        return last - list;
    }

  private:
    Type list[MAX_MOVES];
    Type *last;
};

} // namespace Chess
