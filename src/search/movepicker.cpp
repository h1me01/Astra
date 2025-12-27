#include "movepicker.h"

namespace search {

void partial_insertion_sort(MoveList<ScoredMove> &ml, int idx) {
    assert(idx >= 0);

    int best_idx = idx;
    for(int i = 1 + idx; i < ml.size(); i++)
        if(ml[i].score > ml[best_idx].score)
            best_idx = i;

    std::swap(ml[idx], ml[best_idx]);
}

// MovePicker

MovePicker::MovePicker(SearchType st, const Board &board, const History &history, const Stack *stack, const Move &tt_move)
    : st(st), board(board), history(history), stack(stack) {

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

        killer = (stack->killer != tt_move) ? stack->killer : Move::none();
        counter = (stack - 1)->move ? history.get_counter((stack - 1)->move) : Move::none();

        if(counter == tt_move || counter == killer)
            counter = Move::none();
    }
}

Move MovePicker::next(bool skip_quiets) {
    switch(stage) {
    case PLAY_TT_MOVE:
        stage = GEN_NOISY;
        if(board.pseudo_legal(tt_move))
            return tt_move;
        [[fallthrough]];
    case GEN_NOISY:
        idx = 0;
        stage = PLAY_NOISY;
        gen_score_noisy();
        [[fallthrough]];
    case PLAY_NOISY:
        while(idx < ml_main.size()) {
            partial_insertion_sort(ml_main, idx);

            auto move = ml_main[idx++];
            if(move == tt_move)
                continue;

            // we want to play noisy moves first in qsearch, doesn't matter if its see fails
            int threshold = (st == N_SEARCH) ? -move.score / 32 : probcut_threshold;
            if(st == Q_SEARCH || board.see(move, threshold))
                return move;

            ml_bad_noisy.add(move);
        }

        if(st == PC_SEARCH)
            return Move::none();

        if(st == Q_SEARCH && !board.in_check())
            return Move::none();

        // in evasion qsearch we can falltrough the killer and counter since we did not set them
        stage = PLAY_KILLER;
        [[fallthrough]];
    case PLAY_KILLER:
        stage = PLAY_COUNTER;
        if(!skip_quiets && board.pseudo_legal(killer))
            return killer;
        [[fallthrough]];
    case PLAY_COUNTER:
        stage = GEN_QUIETS;
        if(!skip_quiets && board.pseudo_legal(counter))
            return counter;
        [[fallthrough]];
    case GEN_QUIETS:
        idx = 0;
        stage = PLAY_QUIETS;

        if(!skip_quiets)
            gen_score_quiets();

        [[fallthrough]];
    case PLAY_QUIETS:
        while(idx < ml_main.size() && !skip_quiets) {
            partial_insertion_sort(ml_main, idx);
            Move move = ml_main[idx++];
            if(move != tt_move && move != killer && move != counter)
                return move;
        }

        if(st == Q_SEARCH)
            return Move::none(); // end of evasion qsearch

        idx = 0;
        stage = PLAY_BAD_NOISY;

        [[fallthrough]];
    case PLAY_BAD_NOISY:
        while(idx < ml_bad_noisy.size()) {
            partial_insertion_sort(ml_bad_noisy, idx);
            return ml_bad_noisy[idx++];
        }

        return Move::none();
    default:
        assert(false);
        return Move::none();
    }
}

// private functions

void MovePicker::gen_score_noisy() {
    ml_main.gen<ADD_NOISY>(board);

    for(auto &m : ml_main) {
        const PieceType captured = m.is_ep() ? PAWN : piece_type(board.piece_at(m.to()));
        m.score = history.get_noisy_hist(board, m) + 16 * PIECE_VALUES[captured];
    }
}

void MovePicker::gen_score_quiets() {
    ml_main.gen<ADD_QUIETS>(board);
    Threats threats = board.threats();

    for(auto &m : ml_main) {
        const Piece pc = board.piece_at(m.from());
        const PieceType pt = piece_type(pc);
        const Square to = m.to();

        m.score = 2 * (history.get_heuristic_hist(board.side_to_move(), m) + history.get_pawn_hist(board, m));
        for(int i : {1, 2, 4, 6})
            m.score += static_cast<int>((*(stack - i)->cont_hist)[pc][to]);

        if(pt != PAWN && pt != KING) {
            U64 danger = threats[PAWN];
            if(pt >= ROOK)
                danger |= threats[BISHOP] | threats[KNIGHT];
            if(pt == QUEEN)
                danger |= threats[ROOK];

            const int bonus = (pt == QUEEN) ? 20480 : (pt == ROOK) ? 12288 : 7168;
            if(danger & sq_bb(m.from()))
                m.score += bonus;
            else if(danger & sq_bb(to))
                m.score -= bonus;
        }

        bool can_check = board.check_squares(pt) & sq_bb(to);
        m.score += (can_check && board.see(m, -quiet_checker_bonus)) * 16384;
    }
}

} // namespace search
