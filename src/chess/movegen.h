#pragma once

#include "board.h"

namespace chess {

enum GenType {
    ADD_NOISY = 1,
    ADD_QUIETS = 2,
    ADD_LEGALS = 3,
};

template <GenType gt, Direction d, MoveType mt> //
Move *make_promotions(Move *ml, Square to) {
    assert(valid_sq(to));
    assert(!(to >= a2 && to <= h7));
    assert(mt == PQ_QUEEN || mt == PC_QUEEN);

    if(gt != ADD_QUIETS) {
        *ml++ = Move(to - d, to, mt);
        if(gt == ADD_NOISY && mt == PQ_QUEEN)
            return ml;
    }

    *ml++ = Move(to - d, to, MoveType(mt - 1));
    *ml++ = Move(to - d, to, MoveType(mt - 2));
    *ml++ = Move(to - d, to, MoveType(mt - 3));

    return ml;
}

template <Color us, GenType gt> //
Move *gen_pawn_moves(const Board &board, Move *ml, const U64 targets) {
    constexpr Color them = ~us;
    constexpr U64 rank7_bb = rank_mask(rel_rank(us, RANK_7));
    constexpr Direction up = (us == WHITE ? NORTH : SOUTH);
    constexpr Direction up_right = (us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction up_left = (us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const U64 them_bb = board.occupancy(them);
    const U64 occ = board.occupancy(us) | them_bb;
    const U64 empty_sqs = ~occ;
    const U64 pawns = board.piece_bb(us, PAWN);
    const U64 pawns_non7 = pawns & ~rank7_bb;

    // single and double pawn pushes, no promotions
    if constexpr(gt != ADD_NOISY) {
        U64 b1 = shift<up>(pawns_non7) & empty_sqs;
        U64 b2 = shift<up>(b1 & rank_mask(rel_rank(us, RANK_3))) & empty_sqs & targets;

        b1 &= targets;

        while(b1) {
            Square to = pop_lsb(b1);
            *ml++ = Move(to - up, to, QUIET);
        }

        while(b2) {
            Square to = pop_lsb(b2);
            *ml++ = Move(to - up - up, to, QUIET);
        }
    }

    if constexpr(gt != ADD_QUIETS) {
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
        Square ep_sq = board.en_passant();
        if(valid_sq(ep_sq)) {
            assert(sq_rank(ep_sq) == rel_rank(us, RANK_6));

            b1 = pawns_non7 & pawn_attacks_bb(them, ep_sq);
            while(b1)
                *ml++ = Move(pop_lsb(b1), ep_sq, EN_PASSANT);
        }
    }

    // promotions
    const U64 pawns_on7 = pawns & rank7_bb;

    if(gt != ADD_QUIETS) {
        U64 pb1 = shift<up_right>(pawns_on7) & them_bb & targets;
        while(pb1)
            ml = make_promotions<gt, up_right, PC_QUEEN>(ml, pop_lsb(pb1));

        U64 pb2 = shift<up_left>(pawns_on7) & them_bb & targets;
        while(pb2)
            ml = make_promotions<gt, up_left, PC_QUEEN>(ml, pop_lsb(pb2));
    }

    U64 pb3 = shift<up>(pawns_on7) & empty_sqs & targets;
    while(pb3)
        ml = make_promotions<gt, up, PQ_QUEEN>(ml, pop_lsb(pb3));

    return ml;
}

template <Color us, GenType gt, PieceType pt> //
Move *gen_piece_moves(const Board &board, Move *ml, U64 pieces, const U64 targets) {
    assert(pt != PAWN);

    const Square our_ksq = board.king_sq(us);
    const U64 us_bb = board.occupancy(us);
    const U64 them_bb = board.occupancy(~us);
    const U64 occ = us_bb | them_bb;
    const U64 pinned = board.state().blockers[us] & us_bb;

    while(pieces) {
        Square from = pop_lsb(pieces);
        U64 attacks = attacks_bb(pt, from, occ) & targets;

        if(pinned & sq_bb(from))
            attacks &= line(from, our_ksq);

        if constexpr(gt != ADD_NOISY) {
            U64 target = attacks & ~occ;
            while(target)
                *ml++ = Move(from, pop_lsb(target), QUIET);
        }

        if constexpr(gt != ADD_QUIETS) {
            U64 target = attacks & them_bb;
            while(target)
                *ml++ = Move(from, pop_lsb(target), CAPTURE);
        }
    }

    return ml;
}

template <Color us, GenType gt> //
Move *gen_all(const Board &board, Move *ml) {
    const StateInfo &info = board.state();
    const Square our_ksq = board.king_sq(us);
    const U64 us_bb = board.occupancy(us);
    const U64 them_bb = board.occupancy(~us);
    const U64 occ = us_bb | them_bb;
    const U64 checkers = info.checkers;

    U64 targets = 0;
    if constexpr(gt != ADD_QUIETS)
        targets |= them_bb;
    if constexpr(gt != ADD_NOISY)
        targets |= ~occ;

    ml = gen_piece_moves<us, gt, KING>(board, ml, board.piece_bb(us, KING), targets);

    // if double check, then only king moves are legal
    if(pop_count(checkers) > 1)
        return ml;

    const U64 check_targets = checkers ? between_bb(our_ksq, lsb(checkers)) | checkers : ~0;
    const U64 piece_targets = targets & check_targets;

    ml = gen_pawn_moves<us, gt>(board, ml, check_targets);
    ml = gen_piece_moves<us, gt, KNIGHT>(board, ml, board.piece_bb(us, KNIGHT), piece_targets);
    ml = gen_piece_moves<us, gt, BISHOP>(board, ml, board.diag_sliders(us), piece_targets);
    ml = gen_piece_moves<us, gt, ROOK>(board, ml, board.orth_sliders(us), piece_targets);

    // castling moves
    if((gt != ADD_NOISY) && !checkers) {
        if(info.castling_rights.kingside(us) && !(occ & ks_castle_path_mask(us)))
            *ml++ = Move(rel_sq(us, e1), rel_sq(us, g1), CASTLING);
        if(info.castling_rights.queenside(us) && !(occ & qs_castle_path_mask(us)))
            *ml++ = Move(rel_sq(us, e1), rel_sq(us, c1), CASTLING);
    }

    return ml;
}

inline Move *gen_legals(const Board &board, Move *ml) {
    const Color us = board.side_to_move();
    const Square our_ksq = board.king_sq(us);
    const U64 pinned = board.state().blockers[us] & board.occupancy(us);

    Move *curr = ml;
    ml = (us == WHITE) ? gen_all<WHITE, ADD_LEGALS>(board, ml) : gen_all<BLACK, ADD_LEGALS>(board, ml);

    while(curr != ml) {
        bool needs_checking = (pinned & sq_bb(curr->from())) || curr->is_ep() || curr->from() == our_ksq;
        if(needs_checking && !board.legal(*curr))
            *curr = *(--ml);
        else
            ++curr;
    }

    return ml;
}

template <typename T = Move> //
class MoveList {
  public:
    MoveList() : last(list) {}

    constexpr T &operator[](int i) {
        return list[i];
    }

    constexpr const T &operator[](int i) const {
        return list[i];
    }

    template <GenType gt> //
    void gen(const Board &board) {
        last = list; // reset the list

        auto gen_moves = [&](Move *move_list) -> Move * {
            bool is_w = board.side_to_move() == WHITE;

            if(gt == ADD_LEGALS) {
                return gen_legals(board, move_list);
            } else {
                return is_w ? gen_all<WHITE, gt>(board, move_list) //
                            : gen_all<BLACK, gt>(board, move_list);
            }
        };

        if constexpr(std::is_same_v<T, Move>) {
            last = gen_moves(list);
        } else {
            Move temp_list[MAX_MOVES];
            Move *temp_last = gen_moves(temp_list);

            for(Move *it = temp_list; it != temp_last; ++it)
                *last++ = T(*it);
        }
    }

    void add(T m) {
        assert(last < list + MAX_MOVES);
        *last++ = m;
    }

    T *begin() {
        return list;
    }

    const T *begin() const {
        return list;
    }

    T *end() {
        return last;
    }

    const T *end() const {
        return last;
    }

    int idx_of(const T &move) const {
        for(int i = 0; i < size(); i++)
            if(list[i] == move)
                return i;
        return -1;
    }

    int size() const {
        return last - list;
    }

  private:
    T list[MAX_MOVES];
    T *last;
};

} // namespace chess
