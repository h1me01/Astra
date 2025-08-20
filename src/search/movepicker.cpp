#include "movepicker.h"

namespace Search {

void partial_insertion_sort(MoveList &ml, int idx) {
    assert(idx >= 0);

    int best_idx = idx;
    for(int i = 1 + idx; i < ml.size(); i++)
        if(ml[i].get_score() > ml[best_idx].get_score())
            best_idx = i;

    std::swap(ml[idx], ml[best_idx]);
}

// MovePicker class

MovePicker::MovePicker(SearchType st,          //
                       const Board &board,     //
                       const History &history, //
                       const Stack *s,         //
                       const Move &tt_move)
    : st(st), board(board), history(history), s(s), tt_move(tt_move) {

    if(st == Q_SEARCH) {
        if(board.in_check()) {
            stage = PLAY_TT_MOVE;
            this->tt_move = tt_move;
        } else {
            stage = GEN_NOISY;
        }
    } else {
        stage = PLAY_TT_MOVE;
        this->tt_move = tt_move;

        killer = s->killer;
        counter = (s - 1)->move.is_valid() ? history.get_counter((s - 1)->move) : NO_MOVE;
    }
}

Move MovePicker::next() {
    switch(stage) {
    case PLAY_TT_MOVE:
        stage = GEN_NOISY;
        if(board.is_pseudolegal(tt_move))
            return tt_move;
        [[fallthrough]];
    case GEN_NOISY:
        idx = 0;
        stage = PLAY_NOISY;

        ml_main.gen<NOISY>(board);
        score_noisy();

        [[fallthrough]];
    case PLAY_NOISY:
        while(idx < ml_main.size()) {
            partial_insertion_sort(ml_main, idx);
            Move move = ml_main[idx];
            idx++;

            // skip tt move only when in negamax search
            if(move == tt_move)
                continue;

            // we want to play captures first in qsearch, doesn't matter if its see is fails
            int threshold = -move.get_score() / 32;
            if(st != Q_SEARCH && (move.is_underprom() || !board.see(move, threshold))) {
                ml_bad_noisy.add(move);
                continue;
            }

            return move;
        }

        if(st == Q_SEARCH && !board.in_check())
            return NO_MOVE;

        // in evasion qsearch we can falltrough the killer and counter since we did not set them
        stage = PLAY_KILLER;
        [[fallthrough]];
    case PLAY_KILLER:
        stage = PLAY_COUNTER;
        if(!m_skip_quiets && killer != tt_move && board.is_pseudolegal(killer))
            return killer;
        [[fallthrough]];
    case PLAY_COUNTER:
        stage = GEN_QUIETS;
        if(!m_skip_quiets && counter != tt_move && counter != killer && board.is_pseudolegal(counter))
            return counter;
        [[fallthrough]];
    case GEN_QUIETS:
        idx = 0;
        stage = PLAY_QUIETS;

        if(!m_skip_quiets) {
            ml_main.gen<QUIETS>(board);
            score_quiets();
        }

        [[fallthrough]];
    case PLAY_QUIETS:
        while(idx < ml_main.size() && !m_skip_quiets) {
            partial_insertion_sort(ml_main, idx);
            Move move = ml_main[idx];
            idx++;

            // skip tt move, killer, and counter
            if(move == tt_move || move == killer || move == counter)
                continue;

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
            Move move = ml_bad_noisy[idx];
            idx++;
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
    for(int i = 0; i < ml_main.size(); i++) {
        const Piece pc = board.piece_at(ml_main[i].from());
        const PieceType pt = piece_type(pc);
        const Square from = ml_main[i].from();
        const Square to = ml_main[i].to();

        assert(valid_sq(to));
        assert(valid_sq(from));
        assert(valid_piece(pc));
        assert(valid_piece_type(pt));

        int score = history.get_hh(board.get_stm(), ml_main[i]);

        if(pt != PAWN && pt != KING) {
            U64 danger;
            if(pt == QUEEN)
                danger = board.get_threats(ROOK);
            else if(pt == ROOK)
                danger = board.get_threats(BISHOP) | board.get_threats(KNIGHT);
            else
                danger = board.get_threats(PAWN);

            if(danger & square_bb(from))
                score += 16384 + 16384 * (pt == QUEEN);
            else if(danger & square_bb(to))
                score -= (16384 + 16384 * (pt == QUEEN));
        }

        ml_main[i].set_score(score);
    }
}

void MovePicker::score_noisy() {
    for(int i = 0; i < ml_main.size(); i++) {
        PieceType captured = (ml_main[i].type() == EN_PASSANT) ? PAWN //
                                                               : piece_type(board.piece_at(ml_main[i].to()));

        int score = history.get_nh(board, ml_main[i]);

        score += 16 * PIECE_VALUES[captured];
        score += 4096 * (2 * ml_main[i].is_prom() - ml_main[i].is_underprom());

        ml_main[i].set_score(score);
    }
}

} // namespace Search