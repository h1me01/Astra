#include <algorithm>
#include <cstring>

#include "history.h"
#include "tune_params.h"

namespace search {

int History::history_bonus(int depth) {
    return std::min(int(max_hist_bonus), hist_bonus_mult * depth + hist_bonus_minus);
}

int History::history_malus(int depth) {
    return std::min(int(max_hist_malus), hist_malus_mult * depth + hist_malus_minus);
}

int adjusted_bonus(int value, int bonus) {
    return bonus - value * std::abs(bonus) / 16384;
}

void update_corr(int16_t &value, int diff, int depth) {
    const int bonus = std::clamp(diff * depth / 8, -256, 256);
    value = bonus - int(value) * std::abs(bonus) / 1024;
}

// History

void History::clear() {
    std::memset(heuristic_hist, 0, sizeof(heuristic_hist));
    std::memset(noisy_hist, 0, sizeof(noisy_hist));
    std::memset(pawn_hist, 0, sizeof(pawn_hist));
    std::memset(cont_hist, 0, sizeof(cont_hist));

    std::memset(pawn_corr, 0, sizeof(pawn_corr));
    std::memset(w_non_pawn_corr, 0, sizeof(w_non_pawn_corr));
    std::memset(b_non_pawn_corr, 0, sizeof(b_non_pawn_corr));
    std::memset(cont_corr, 0, sizeof(cont_corr));

    std::fill(&counters[0][0], &counters[0][0] + NUM_SQUARES * NUM_SQUARES, Move{});
}

void History::update(         //
    const Board &board,       //
    const Move &best_move,    //
    const MoveList<> &quiets, //
    const MoveList<> &noisy,  //
    Stack *stack,             //
    int depth                 //
) {
    const Color stm = board.get_stm();
    const int bonus = history_bonus(depth);
    const int malus = history_malus(depth);

    if(best_move.is_quiet()) {
        Move prev_move = (stack - 1)->move;
        if(prev_move)
            counters[prev_move.from()][prev_move.to()] = best_move;

        stack->killer = best_move;

        // idea from ethereal
        // only update quiet history if best move was important
        if(depth > 3 || quiets.size() > 1) {
            const int quiet_bonus = bonus * quiet_hist_bonus_mult / 1024;
            const int quiet_malus = malus * quiet_hist_malus_mult / 1024;

            update_quiet_hist(stm, best_move, quiet_bonus);
            update_pawn_hist(board, best_move, quiet_bonus);
            update_cont_hist(board.piece_at(best_move.from()), best_move.to(), stack, quiet_bonus);

            for(const auto &m : quiets) {
                update_quiet_hist(stm, m, -quiet_malus);
                update_pawn_hist(board, m, -quiet_malus);
                update_cont_hist(board.piece_at(m.from()), m.to(), stack, -quiet_malus);
            }
        }
    } else {
        update_noisy_hist(board, best_move, bonus * noisy_hist_bonus_mult / 1024);
    }

    for(const auto &m : noisy)
        update_noisy_hist(board, m, -malus * noisy_hist_malus_mult / 1024);
}

void History::update_quiet_hist(Color c, const Move &move, int bonus) {
    assert(move);
    int16_t &value = heuristic_hist[c][move.from()][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_noisy_hist(const Board &board, const Move &move, int bonus) {
    assert(move);

    Piece pc = board.piece_at(move.from());
    PieceType captured = piece_type(board.captured_piece(move));

    assert(valid_piece(pc));
    assert(valid_piece_type(captured) || move.is_prom());

    int16_t &value = noisy_hist[pc][move.to()][captured];
    value += adjusted_bonus(value, bonus);
}

void History::update_pawn_hist(const Board &board, const Move &move, int bonus) {
    assert(move);
    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));
    int16_t &value = pawn_hist[ph_idx(board.get_pawn_hash())][pc][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_cont_hist(const Piece pc, const Square to, Stack *stack, int bonus) {
    assert(valid_piece(pc));
    assert(valid_sq(to));

    for(int i : {1, 2, 4, 6}) {
        int16_t &value = (*(stack - i)->cont_hist)[pc][to];
        value += adjusted_bonus(value, bonus);
    }
}

void History::update_mat_corr(const Board &board, Score raw_eval, Score real_score, int depth) {
    const Color stm = board.get_stm();
    const int diff = real_score - raw_eval;

    update_corr(pawn_corr[stm][corr_idx(board.get_pawn_hash())], diff, depth);
    update_corr(w_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(WHITE))], diff, depth);
    update_corr(b_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(BLACK))], diff, depth);
}

void History::update_cont_corr(Score raw_eval, Score real_score, int depth, const Stack *stack) {
    const Move prev_move = (stack - 1)->move;
    const Move pprev_move = (stack - 2)->move;

    const Piece prev_pc = (stack - 1)->moved_piece;
    const Piece pprev_pc = (stack - 2)->moved_piece;
    const int diff = real_score - raw_eval;

    if(prev_move && pprev_move)
        update_corr(cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()], diff, depth);
}

} // namespace search
