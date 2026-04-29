#include <utility>

#include "movepicker.h"

namespace astra::search {

void select_best(MoveList<ScoredMove>& ml, int idx) {
    assert(idx >= 0);

    int best_idx = idx;
    for (int i = 1 + idx; i < ml.size(); i++)
        if (ml[i].score > ml[best_idx].score)
            best_idx = i;

    std::swap(ml[idx], ml[best_idx]);
}

// MovePicker

template <SearchType st>
MovePicker<st>::MovePicker(
    const Board& board,
    const Move tt_move,
    const QuietHistory& quiet_history,
    const PawnHistory& pawn_history,
    const NoisyHistory& noisy_history,
    const Stack* stack,
    int probcut_threshold
)
    : board_(board),
      quiet_history_(quiet_history),
      pawn_history_(pawn_history),
      noisy_history_(noisy_history),
      stack_(stack),
      probcut_threshold_(probcut_threshold) {

    bool use_tt = (st == N_SEARCH) || (st == Q_SEARCH && board.in_check());
    stage_ = use_tt ? PLAY_TT_MOVE : GEN_NOISY;
    tt_move_ = use_tt ? tt_move : Move::none();
}

template <SearchType st>
Move MovePicker<st>::next(bool skip_quiets) {
    switch (stage_) {
    case PLAY_TT_MOVE:
        stage_ = GEN_NOISY;
        if (board_.is_pseudo_legal(tt_move_))
            return tt_move_;
        [[fallthrough]];
    case GEN_NOISY:
        idx_ = 0;
        stage_ = PLAY_NOISY;
        gen_score_noisy();
        [[fallthrough]];
    case PLAY_NOISY:
        while (idx_ < ml_main_.size()) {
            select_best(ml_main_, idx_);

            auto move = ml_main_[idx_++];
            if (move == tt_move_)
                continue;

            // we want to play noisy moves first in qsearch, doesn't matter if its see fails
            int threshold = (st == N_SEARCH) ? -move.score / 32 : probcut_threshold_;
            if (st == Q_SEARCH || board_.see(move, threshold))
                return move;

            ml_bad_noisy_.add(move);
        }

        if (st == PC_SEARCH)
            return Move::none();

        if (st == Q_SEARCH && !board_.in_check())
            return Move::none();

        stage_ = GEN_QUIETS;
        [[fallthrough]];
    case GEN_QUIETS:
        idx_ = 0;
        stage_ = PLAY_QUIETS;

        if (!skip_quiets)
            gen_score_quiets();

        [[fallthrough]];
    case PLAY_QUIETS:
        while (idx_ < ml_main_.size() && !skip_quiets) {
            select_best(ml_main_, idx_);
            Move move = ml_main_[idx_++];
            if (move != tt_move_)
                return move;
        }

        if (st == Q_SEARCH)
            return Move::none(); // end of evasion qsearch

        idx_ = 0;
        stage_ = PLAY_BAD_NOISY;

        [[fallthrough]];
    case PLAY_BAD_NOISY:
        while (idx_ < ml_bad_noisy_.size()) {
            select_best(ml_bad_noisy_, idx_);
            return ml_bad_noisy_[idx_++];
        }

        return Move::none();
    default:
        assert(false);
        return Move::none();
    }
}

// private functions

template <SearchType st>
void MovePicker<st>::gen_score_noisy() {
    ml_main_.gen<GenType::NOISY>(board_);

    for (auto& m : ml_main_) {
        PieceType captured = m.is_ep() ? PAWN : piece_type(board_.piece_at(m.to()));
        m.score = noisy_history_.get(board_, m) + 16 * piece_values(captured);
    }
}

template <SearchType st>
void MovePicker<st>::gen_score_quiets() {
    ml_main_.gen<GenType::QUIET>(board_);
    Threats threats = board_.threats();

    for (auto& m : ml_main_) {
        const Piece pc = board_.piece_at(m.from());
        const PieceType pt = piece_type(pc);
        const Square to = m.to();

        m.score = 2 * (quiet_history_.get(board_.side_to_move(), m) + pawn_history_.get(board_, m));
        for (int i : {1, 2, 4, 6})
            m.score += static_cast<int>((*(stack_ - i)->cont_hist)[pc][to]);

        if (pt != PAWN && pt != KING) {
            Bitboard danger = threats(PAWN);
            if (pt == ROOK || pt == QUEEN)
                danger |= threats(BISHOP) | threats(KNIGHT);
            if (pt == QUEEN)
                danger |= threats(ROOK);

            const int bonus = (pt == QUEEN) ? 20480 : (pt == ROOK) ? 12288 : 7168;
            if (danger & sq_bb(m.from()))
                m.score += bonus;
            else if (danger & sq_bb(to))
                m.score -= bonus;
        }

        bool can_check = board_.check_squares(pt) & sq_bb(to);
        m.score += (can_check && board_.see(m, -quiet_checker_bonus)) * 16384;
    }
}

template class MovePicker<N_SEARCH>;
template class MovePicker<Q_SEARCH>;
template class MovePicker<PC_SEARCH>;

} // namespace astra::search
