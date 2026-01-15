#include "movegen.h"

namespace chess {

template <GenType gt, Direction d, MoveType mt>
Move* make_promotions(Move* ml, Square to) {
    assert(valid_sq(to));
    assert(!(to >= a2 && to <= h7));
    assert(mt == PQ_QUEEN || mt == PC_QUEEN);

    if (gt != ADD_QUIETS)
        *ml++ = Move(to - d, to, mt);

    if ((gt == ADD_NOISY && mt == PQ_QUEEN) || (gt == ADD_QUIETS && mt == PC_QUEEN))
        return ml;

    *ml++ = Move(to - d, to, MoveType(mt - 1));
    *ml++ = Move(to - d, to, MoveType(mt - 2));
    *ml++ = Move(to - d, to, MoveType(mt - 3));

    return ml;
}

template <Color us, GenType gt>
Move* gen_pawn_moves(const Board& board, Move* ml, const U64 targets) {
    constexpr Color them = ~us;
    constexpr U64 rank7_bb = rank_bb(rel_rank(us, RANK_7));
    constexpr Direction up = (us == WHITE ? NORTH : SOUTH);
    constexpr Direction up_right = (us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction up_left = (us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const U64 them_bb = board.occupancy(them);
    const U64 occ = board.occupancy(us) | them_bb;
    const U64 empty_sqs = ~occ;
    const U64 pawns = board.piece_bb<PAWN>(us);
    const U64 pawns_non7 = pawns & ~rank7_bb;
    const U64 pawns_on7 = pawns & rank7_bb;
    const U64 checkers = board.state().checkers;

    // single and double pawn pushes, no promotions
    if constexpr (gt != ADD_NOISY) {
        U64 b1 = shift<up>(pawns_non7) & empty_sqs;
        U64 b2 = shift<up>(b1 & rank_bb(rel_rank(us, RANK_3))) & empty_sqs & targets;

        b1 &= targets;

        while (b1) {
            Square to = pop_lsb(b1);
            *ml++ = Move(to - up, to, QUIET);
        }

        while (b2) {
            Square to = pop_lsb(b2);
            *ml++ = Move(to - up - up, to, QUIET);
        }
    }

    if constexpr (gt != ADD_QUIETS) {
        // standard captures
        U64 b = shift<up_right>(pawns_non7) & them_bb & targets;
        while (b) {
            Square to = pop_lsb(b);
            *ml++ = Move(to - up_right, to, CAPTURE);
        }

        b = shift<up_left>(pawns_non7) & them_bb & targets;
        while (b) {
            Square to = pop_lsb(b);
            *ml++ = Move(to - up_left, to, CAPTURE);
        }

        // en passant
        Square ep_sq = board.en_passant();
        if (valid_sq(ep_sq) && !(board.in_check() && ((targets ^ checkers) & sq_bb(ep_sq + up)))) {
            assert(sq_rank(ep_sq) == rel_rank(us, RANK_6));

            b = pawns_non7 & attacks_bb(them, ep_sq);
            while (b)
                *ml++ = Move(pop_lsb(b), ep_sq, EN_PASSANT);
        }
    }

    // promotions
    if (pawns_on7) {
        U64 b = shift<up_right>(pawns_on7) & them_bb & targets;
        while (b)
            ml = make_promotions<gt, up_right, PC_QUEEN>(ml, pop_lsb(b));

        b = shift<up_left>(pawns_on7) & them_bb & targets;
        while (b)
            ml = make_promotions<gt, up_left, PC_QUEEN>(ml, pop_lsb(b));

        b = shift<up>(pawns_on7) & empty_sqs & targets;
        while (b)
            ml = make_promotions<gt, up, PQ_QUEEN>(ml, pop_lsb(b));
    }

    return ml;
}

template <Color us, GenType gt, PieceType pt>
Move* gen_piece_moves(const Board& board, Move* ml, U64 pieces, const U64 targets) {
    assert(pt != PAWN);

    const Square our_ksq = board.king_sq(us);
    const U64 us_bb = board.occupancy(us);
    const U64 them_bb = board.occupancy(~us);
    const U64 occ = us_bb | them_bb;
    const U64 pinned = board.state().blockers[us] & us_bb;

    while (pieces) {
        Square from = pop_lsb(pieces);
        U64 attacks = attacks_bb(pt, from, occ) & targets;

        if (pinned & sq_bb(from))
            attacks &= line(from, our_ksq);

        while (attacks) {
            Square to = pop_lsb(attacks);
            *ml++ = Move(from, to, (them_bb & sq_bb(to)) ? CAPTURE : QUIET);
        }
    }

    return ml;
}

template <Color us, GenType gt>
Move* gen_all_moves(const Board& board, Move* ml) {
    const StateInfo& info = board.state();
    const Square our_ksq = board.king_sq(us);
    const U64 us_bb = board.occupancy(us);
    const U64 them_bb = board.occupancy(~us);
    const U64 occ = us_bb | them_bb;
    const U64 checkers = info.checkers;

    U64 targets = 0;
    if constexpr (gt != ADD_QUIETS)
        targets |= them_bb;
    if constexpr (gt != ADD_NOISY)
        targets |= ~occ;

    ml = gen_piece_moves<us, gt, KING>(board, ml, board.piece_bb<KING>(us), targets);

    // if double check, then only king moves are legal
    if (pop_count(checkers) > 1)
        return ml;

    const U64 check_targets = checkers ? between_bb(our_ksq, lsb(checkers)) | checkers : ~0;
    const U64 piece_targets = targets & check_targets;

    ml = gen_pawn_moves<us, gt>(board, ml, check_targets);
    ml = gen_piece_moves<us, gt, KNIGHT>(board, ml, board.piece_bb<KNIGHT>(us), piece_targets);
    ml = gen_piece_moves<us, gt, BISHOP>(board, ml, board.diag_sliders(us), piece_targets);
    ml = gen_piece_moves<us, gt, ROOK>(board, ml, board.orth_sliders(us), piece_targets);

    // castling moves
    if ((gt != ADD_NOISY) && !checkers) {
        if (info.castling_rights.kingside(us) && !(occ & ks_castle_path_bb(us)))
            *ml++ = Move(rel_sq(us, e1), rel_sq(us, g1), CASTLING);
        if (info.castling_rights.queenside(us) && !(occ & qs_castle_path_bb(us)))
            *ml++ = Move(rel_sq(us, e1), rel_sq(us, c1), CASTLING);
    }

    return ml;
}

template <GenType gt>
Move* gen_moves(const Board& board, Move* ml) {
    Color us = board.side_to_move();
    return (us == WHITE) ? gen_all_moves<WHITE, gt>(board, ml) : gen_all_moves<BLACK, gt>(board, ml);
}

template <>
Move* gen_moves<ADD_LEGALS>(const Board& board, Move* ml) {
    const Color us = board.side_to_move();
    const Square our_ksq = board.king_sq(us);
    const U64 pinned = board.state().blockers[us] & board.occupancy(us);

    Move* curr = ml;
    ml = (us == WHITE) ? gen_all_moves<WHITE, ADD_LEGALS>(board, ml) : gen_all_moves<BLACK, ADD_LEGALS>(board, ml);

    while (curr != ml) {
        bool needs_checking = (pinned & sq_bb(curr->from())) || curr->is_ep() || curr->from() == our_ksq;
        if (needs_checking && !board.legal(*curr))
            *curr = *(--ml);
        else
            ++curr;
    }

    return ml;
}

template Move* gen_moves<ADD_NOISY>(const Board& board, Move* ml);
template Move* gen_moves<ADD_QUIETS>(const Board& board, Move* ml);

} // namespace chess
