#include "movepicker.h"

namespace Engine {

void partial_insertion_sort(MoveList<> &ml, int idx) {
    assert(idx >= 0);

    int best_idx = idx;
    for(int i = 1 + idx; i < ml.size(); i++)
        if(ml[i].get_score() > ml[best_idx].get_score())
            best_idx = i;

    std::swap(ml[idx], ml[best_idx]);
}

// MovePicker

MovePicker::MovePicker(SearchType st,          //
                       const Board &board,     //
                       const History &history, //
                       const Stack *stack,     //
                       const Move &tt_move,    //
                       bool gen_checks)
    : st(st), board(board), history(history), stack(stack), gen_checks(gen_checks) {

    if(st == PC_SEARCH)
        stage = GEN_NOISY;
    else if(st == Q_SEARCH) {
        if(board.in_check()) {
            stage = PLAY_TT_MOVE;
            this->tt_move = tt_move;
        } else
            stage = GEN_NOISY;
    } else {
        stage = PLAY_TT_MOVE;

        this->tt_move = tt_move;

        killer = (stack->killer != tt_move) ? stack->killer : NO_MOVE;
        counter = (stack - 1)->move ? history.get_counter((stack - 1)->move) : NO_MOVE;

        if(counter == tt_move || counter == killer)
            counter = NO_MOVE;
    }
}

Move MovePicker::next(bool skip_quiets) {
    switch(stage) {
    case PLAY_TT_MOVE:
        stage = GEN_NOISY;
        if(board.is_pseudo_legal(tt_move))
            return tt_move;
        [[fallthrough]];
    case GEN_NOISY:
        idx = 0;
        stage = PLAY_NOISY;

        score_noisy();

        [[fallthrough]];
    case PLAY_NOISY:
        while(idx < ml_main.size()) {
            partial_insertion_sort(ml_main, idx);

            Move move = ml_main[idx++];
            if(move == tt_move)
                continue;

            // we want to play noisy moves first in qsearch, doesn't matter if its see fails
            int threshold = (st == N_SEARCH) ? -move.get_score() / 32 : probcut_threshold;
            if(st == Q_SEARCH || board.see(move, threshold))
                return move;

            ml_bad_noisy.add(move);
        }

        if(st == PC_SEARCH)
            return NO_MOVE;

        if(st == Q_SEARCH && !board.in_check()) {
            if(!gen_checks)
                return NO_MOVE;
            goto quiet_checkers;
        }

        // in evasion qsearch we can falltrough the killer and counter since we did not set them
        stage = PLAY_KILLER;
        [[fallthrough]];
    case PLAY_KILLER:
        stage = PLAY_COUNTER;
        if(!skip_quiets && board.is_pseudo_legal(killer))
            return killer;
        [[fallthrough]];
    case PLAY_COUNTER:
        stage = GEN_QUIETS;
        if(!skip_quiets && board.is_pseudo_legal(counter))
            return counter;
        [[fallthrough]];
    case GEN_QUIETS:
        idx = 0;
        stage = PLAY_QUIETS;

        if(!skip_quiets)
            score_quiets();

        [[fallthrough]];
    case PLAY_QUIETS:
        while(idx < ml_main.size() && !skip_quiets) {
            partial_insertion_sort(ml_main, idx);
            Move move = ml_main[idx++];
            if(move != tt_move && move != killer && move != counter)
                return move;
        }

        if(st == Q_SEARCH)
            return NO_MOVE; // end of evasion qsearch

        idx = 0;
        stage = PLAY_BAD_NOISY;

        [[fallthrough]];
    case PLAY_BAD_NOISY:
        while(idx < ml_bad_noisy.size()) {
            partial_insertion_sort(ml_bad_noisy, idx);
            Move move = ml_bad_noisy[idx++];
            return move;
        }

        return NO_MOVE;
    case GEN_QUIET_CHECKERS:
    quiet_checkers:
        idx = 0;
        stage = PLAY_QUIET_CHECKERS;
        ml_main.gen<QUIET_CHECKS>(board);
        [[fallthrough]];
    case PLAY_QUIET_CHECKERS:
        while(idx < ml_main.size()) {
            partial_insertion_sort(ml_main, idx);
            Move move = ml_main[idx++];
            return move;
        }

        return NO_MOVE;
    default:
        assert(false);
        return NO_MOVE;
    }
}

// private member

void MovePicker::score_quiets() {
    ml_main.gen<QUIETS>(board);

    Threats threats = board.get_threats();

    for(int i = 0; i < ml_main.size(); i++) {
        const Move move = ml_main[i];
        const Piece pc = board.piece_at(move.from());
        const PieceType pt = piece_type(pc);
        const Square from = move.from();
        const Square to = move.to();

        assert(move);
        assert(valid_piece(pc));
        assert(valid_piece_type(pt));

        int score = 2 * (history.get_hh(board.get_stm(), move) + history.get_ph(board, move));
        for(int i : {1, 2, 4, 6})
            score += (int) (*(stack - i)->conth)[pc][to];

        if(pt != PAWN && pt != KING) {
            U64 danger;
            if(pt == QUEEN)
                danger = threats[ROOK];
            else if(pt == ROOK)
                danger = threats[BISHOP] | threats[KNIGHT];
            else
                danger = threats[PAWN];

            const int bonus = 16384 + 16384 * (pt == QUEEN);
            if(danger & sq_bb(from))
                score += bonus;
            else if(danger & sq_bb(to))
                score -= bonus;
        }

        ml_main[i].set_score(score);
    }
}

void MovePicker::score_noisy() {
    ml_main.gen<NOISY>(board);

    for(int i = 0; i < ml_main.size(); i++) {
        PieceType captured = ml_main[i].is_ep() ? PAWN : piece_type(board.piece_at(ml_main[i].to()));
        int score = history.get_nh(board, ml_main[i]);

        score += 16 * PIECE_VALUES[captured];
        score += 4096 * (2 * ml_main[i].is_prom() - ml_main[i].is_underprom());

        ml_main[i].set_score(score);
    }
}

} // namespace Engine
