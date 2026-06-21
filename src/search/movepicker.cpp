#include <utility>

#include "movepicker.h"
#include "tune_params.h"

namespace astra::search {

void select_best(MoveList<ScoredMove>& ml, int idx) {
    assert(idx >= 0);
    int best_idx = idx;
    for (int i = 1 + idx; i < ml.size(); ++i)
        if (ml[i].score > ml[best_idx].score)
            best_idx = i;
    std::swap(ml[idx], ml[best_idx]);
}

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

    bool use_tt = (st == SearchType::NEGAMAX) || (st == SearchType::QUIESCENCE && board.in_check());
    stage_ = use_tt ? Stage::PLAY_TT_MOVE : Stage::GEN_NOISY;
    tt_move_ = use_tt ? tt_move : Move::none();
}

template <SearchType st>
Move MovePicker<st>::next() {
    switch (stage_) {
    case Stage::PLAY_TT_MOVE:
        stage_ = Stage::GEN_NOISY;
        if (board_.is_pseudo_legal(tt_move_))
            return tt_move_;
        [[fallthrough]];
    case Stage::GEN_NOISY:
        stage_ = Stage::PLAY_NOISY;
        gen_score_noisy();
        [[fallthrough]];
    case Stage::PLAY_NOISY:
        while (curr_move_idx_ < ml_main_.size()) {
            select_best(ml_main_, curr_move_idx_);

            auto move = ml_main_[curr_move_idx_++];
            if (move == tt_move_)
                continue;

            // we want to play noisy moves first in qsearch, doesn't matter if its see fails
            int threshold = (st == SearchType::NEGAMAX) ? -move.score / 32 : probcut_threshold_;
            if (st == SearchType::QUIESCENCE || board_.see(move, threshold))
                return move;

            // overwrite movelist starting from 0 with bad noisy
            ml_main_[bad_noisy_count_++] = move;
        }

        if (st == SearchType::PROBCUT)
            return Move::none();
        if (st == SearchType::QUIESCENCE && !board_.in_check())
            return Move::none();

        stage_ = Stage::GEN_QUIETS;
        [[fallthrough]];
    case Stage::GEN_QUIETS:
        stage_ = Stage::PLAY_QUIETS;
        if (!skip_quiets_)
            gen_score_quiets();
        [[fallthrough]];
    case Stage::PLAY_QUIETS:
        while (curr_move_idx_ < ml_main_.size() && !skip_quiets_) {
            select_best(ml_main_, curr_move_idx_);
            Move move = ml_main_[curr_move_idx_++];
            if (move != tt_move_)
                return move;
        }

        if (st == SearchType::QUIESCENCE)
            return Move::none(); // end of evasion qsearch

        stage_ = Stage::PLAY_BAD_NOISY;

        [[fallthrough]];
    case Stage::PLAY_BAD_NOISY:
        if (bad_noisy_idx_ < bad_noisy_count_)
            return ml_main_[bad_noisy_idx_++];
        return Move::none();
    default:
        assert(false);
        return Move::none();
    }
}

template <SearchType st>
void MovePicker<st>::gen_score_noisy() {
    MoveList<Move> ml;
    gen_moves<GenType::NOISY>(ml, board_);

    for (auto& m : ml) {
        int score = noisy_history_.get(board_, m) + 16 * piece_values(type_of(board_.capture_piece(m)));
        ml_main_.add(ScoredMove{m, score});
    }
}

template <SearchType st>
void MovePicker<st>::gen_score_quiets() {
    MoveList<Move> ml;
    gen_moves<GenType::QUIET>(ml, board_);

    const Color stm = board_.side_to_move();

    NDArray<Bitboard, NUM_PIECE_TYPES> threats;
    threats(KNIGHT) = threats(BISHOP) = board_.attacks_by<PAWN>(~stm);
    threats(ROOK) = board_.attacks_by<KNIGHT>(~stm) | board_.attacks_by<BISHOP>(~stm) | threats(KNIGHT);
    threats(QUEEN) = board_.attacks_by<ROOK>(~stm) | threats(ROOK);

    for (auto& m : ml) {
        const Square from = m.from();
        const Square to = m.to();
        const Piece pc = board_.piece_at(from);
        const PieceType pt = type_of(pc);

        int score = 2 * (quiet_history_.get(stm, m) + pawn_history_.get(board_, m));
        for (int i : {1, 2, 4, 6})
            score += (*(stack_ - i)->cont_hist)(pc, to);

        score += mp_threat_mul * piece_values(pt) *
                 (static_cast<bool>(threats(pt) & sq_bb(from)) - static_cast<bool>(threats(pt) & sq_bb(to)));

        if (board_.check_squares(pt) & sq_bb(to))
            score += board_.see(m, -quiet_checker_bonus) * 16384;

        ml_main_.add(ScoredMove{m, score});
    }
}

template class MovePicker<SearchType::NEGAMAX>;
template class MovePicker<SearchType::QUIESCENCE>;
template class MovePicker<SearchType::PROBCUT>;

} // namespace astra::search
