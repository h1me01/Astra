#include <iostream>

#include "../util.h"
#include "movegen.h"

namespace astra {

template <GenType gt, Direction d, MoveType mt>
void make_promotions(MoveList<Move>& ml, Square to) {
    assert(is_valid(to));
    assert(mt == PQ_QUEEN || mt == PC_QUEEN);
    assert(rank_of(to) == RANK_1 || rank_of(to) == RANK_8);

    if (gt != GenType::QUIET)
        ml.add({to - d, to, mt});

    if ((gt == GenType::NOISY && mt == PQ_QUEEN) || (gt == GenType::QUIET && mt == PC_QUEEN))
        return;

    ml.add({to - d, to, static_cast<MoveType>(mt - 1)});
    ml.add({to - d, to, static_cast<MoveType>(mt - 2)});
    ml.add({to - d, to, static_cast<MoveType>(mt - 3)});
}

template <Color us, GenType gt>
void gen_pawn_moves(MoveList<Move>& ml, const Board& board, const Bitboard targets) {
    constexpr Color them = ~us;
    constexpr Bitboard rank7_bb = rank_bb(relative_rank(us, RANK_7));
    constexpr Direction up = (us == WHITE ? NORTH : SOUTH);
    constexpr Direction up_right = (us == WHITE ? NORTH_EAST : SOUTH_WEST);
    constexpr Direction up_left = (us == WHITE ? NORTH_WEST : SOUTH_EAST);

    const Bitboard them_bb = board.occupancy(them);
    const Bitboard occ = board.occupancy(us) | them_bb;
    const Bitboard empty_sqs = ~occ;
    const Bitboard pawns = board.piece_bb<PAWN>(us);
    const Bitboard pawns_non7 = pawns & ~rank7_bb;
    const Bitboard pawns_on7 = pawns & rank7_bb;
    const Bitboard checkers = board.state().checkers;

    // single and double pawn pushes, no promotions
    if constexpr (gt != GenType::NOISY) {
        Bitboard b1 = shift<up>(pawns_non7) & empty_sqs;
        Bitboard b2 = shift<up>(b1 & rank_bb(relative_rank(us, RANK_3))) & empty_sqs & targets;

        b1 &= targets;

        while (b1) {
            Square to = pop_lsb(b1);
            ml.add({to - up, to, QUIET});
        }

        while (b2) {
            Square to = pop_lsb(b2);
            ml.add({to - up - up, to, QUIET});
        }
    }

    if constexpr (gt != GenType::QUIET) {
        // standard captures
        Bitboard b = shift<up_right>(pawns_non7) & them_bb & targets;
        while (b) {
            Square to = pop_lsb(b);
            ml.add({to - up_right, to, CAPTURE});
        }

        b = shift<up_left>(pawns_non7) & them_bb & targets;
        while (b) {
            Square to = pop_lsb(b);
            ml.add({to - up_left, to, CAPTURE});
        }

        // en passant
        Square ep_sq = board.en_passant();
        if (is_valid(ep_sq) && !(board.in_check() && ((targets ^ checkers) & sq_bb(ep_sq + up)))) {
            assert(rank_of(ep_sq) == relative_rank(us, RANK_6));

            b = pawns_non7 & pawn_attacks_bb(them, ep_sq);
            while (b)
                ml.add({pop_lsb(b), ep_sq, EN_PASSANT});
        }
    }

    // promotions
    if (pawns_on7) {
        Bitboard b = shift<up_right>(pawns_on7) & them_bb & targets;
        while (b)
            make_promotions<gt, up_right, PC_QUEEN>(ml, pop_lsb(b));

        b = shift<up_left>(pawns_on7) & them_bb & targets;
        while (b)
            make_promotions<gt, up_left, PC_QUEEN>(ml, pop_lsb(b));

        b = shift<up>(pawns_on7) & empty_sqs & targets;
        while (b)
            make_promotions<gt, up, PQ_QUEEN>(ml, pop_lsb(b));
    }
}

template <Color us, PieceType pt>
void gen_piece_moves(MoveList<Move>& ml, const Board& board, Bitboard pieces, const Bitboard targets) {
    assert(pt != PAWN);
    assert(is_valid(pt));

    const Square our_ksq = board.king_sq(us);
    const Bitboard us_bb = board.occupancy(us);
    const Bitboard them_bb = board.occupancy(~us);
    const Bitboard occ = us_bb | them_bb;
    const Bitboard pinned = board.state().blockers(us) & us_bb;

    while (pieces) {
        Square from = pop_lsb(pieces);
        Bitboard attacks = attacks_bb(pt, from, occ) & targets;

        if (pinned & sq_bb(from))
            attacks &= line(from, our_ksq);

        while (attacks) {
            Square to = pop_lsb(attacks);
            ml.add({from, to, (them_bb & sq_bb(to)) ? CAPTURE : QUIET});
        }
    }
}

template <Color us, GenType gt>
void gen_all_moves(MoveList<Move>& ml, const Board& board) {
    const StateInfo& info = board.state();
    const Square our_ksq = board.king_sq(us);
    const Bitboard us_bb = board.occupancy(us);
    const Bitboard them_bb = board.occupancy(~us);
    const Bitboard occ = us_bb | them_bb;
    const Bitboard checkers = info.checkers;

    Bitboard targets = 0;
    if constexpr (gt != GenType::QUIET)
        targets |= them_bb;
    if constexpr (gt != GenType::NOISY)
        targets |= ~occ;

    gen_piece_moves<us, KING>(ml, board, board.piece_bb<KING>(us), targets);

    // if double check, then only king moves are legal
    if (pop_count(checkers) > 1)
        return;

    const Bitboard check_targets = checkers ? between_bb(our_ksq, lsb(checkers)) | checkers : ~Bitboard(0);
    const Bitboard piece_targets = targets & check_targets;

    gen_pawn_moves<us, gt>(ml, board, check_targets);
    gen_piece_moves<us, KNIGHT>(ml, board, board.piece_bb<KNIGHT>(us), piece_targets);
    gen_piece_moves<us, BISHOP>(ml, board, board.diag_sliders(us), piece_targets);
    gen_piece_moves<us, ROOK>(ml, board, board.orth_sliders(us), piece_targets);

    // castling moves
    if ((gt != GenType::NOISY) && !checkers) {
        if (info.castling_rights.kingside(us) && !(occ & kingside_castling_path_bb(us)))
            ml.add({relative_sq(us, SQ_E1), relative_sq(us, SQ_G1), CASTLING});
        if (info.castling_rights.queenside(us) && !(occ & queenside_castling_path_bb(us)))
            ml.add({relative_sq(us, SQ_E1), relative_sq(us, SQ_C1), CASTLING});
    }
}

template <GenType gt>
void gen_moves(MoveList<Move>& ml, const Board& board) {
    ml.clear();

    Color us = board.side_to_move();
    if (us == WHITE)
        gen_all_moves<WHITE, gt>(ml, board);
    else
        gen_all_moves<BLACK, gt>(ml, board);
}

template <>
void gen_moves<GenType::LEGAL>(MoveList<Move>& ml, const Board& board) {
    const Color us = board.side_to_move();
    const Square our_ksq = board.king_sq(us);
    const Bitboard pinned = board.state().blockers(us) & board.occupancy(us);

    ml.clear();
    if (us == WHITE)
        gen_all_moves<WHITE, GenType::LEGAL>(ml, board);
    else
        gen_all_moves<BLACK, GenType::LEGAL>(ml, board);

    Move* curr = ml.begin();

    while (curr != ml.end()) {
        bool needs_checking = (pinned & sq_bb(curr->from())) || curr->is_ep() || curr->from() == our_ksq;
        if (needs_checking && !board.is_legal(*curr)) {
            *curr = ml.back();
            ml.pop();
        } else {
            ++curr;
        }
    }
}

template void gen_moves<GenType::NOISY>(MoveList<Move>& ml, const Board& board);
template void gen_moves<GenType::QUIET>(MoveList<Move>& ml, const Board& board);

} // namespace astra
