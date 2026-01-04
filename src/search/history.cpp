#include <algorithm>
#include <cstring>

#include "history.h"
#include "tune_params.h"

namespace search {

// Helper

int adjusted_bonus(int v, int bonus) {
    return bonus - v * std::abs(bonus) / 16384;
}

void update_correction(int16_t &v, int diff, int d) {
    int bonus = std::clamp(diff * d / 8, -256, 256);
    v = bonus - int(v) * std::abs(bonus) / 1024;
}

// Quiet Histories

void QuietHistory::update(Color c, Move move, int bonus) {
    assert(move);

    auto &value = data[c][move.from()][move.to()];
    value += adjusted_bonus(value, bonus);
}

// Noisy History

void NoisyHistory::update(const Board &board, Move move, int bonus) {
    assert(move);

    Piece pc = board.piece_at(move.from());
    PieceType captured = piece_type(board.captured_piece(move));

    assert(valid_piece(pc));
    assert(valid_piece_type(captured) || move.is_prom());

    auto &value = data[pc][move.to()][captured];
    value += adjusted_bonus(value, bonus);
}

// Pawn History

void PawnHistory::update(const Board &board, Move move, int bonus) {
    assert(move);

    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));

    auto &value = data[idx(board.pawn_hash())][pc][move.to()];
    value += adjusted_bonus(value, bonus);
}

// Continuation History

void ContinuationHistory::update(Piece pc, Square to, int bonus, Stack *stack) {
    assert(valid_piece(pc));
    assert(valid_sq(to));

    for(int i : {1, 2, 4, 6}) {
        auto &value = (*(stack - i)->cont_hist)[pc][to];
        value += adjusted_bonus(value, bonus);
    }
}

// Correction Histories

void CorrectionHistories::update(const Board &board, Score eval, Score score, int d) {
    Color stm = board.side_to_move();
    int diff = score - eval;

    update_correction(pawn[stm][idx(board.pawn_hash())], diff, d);
    update_correction(w_non_pawn[stm][idx(board.non_pawn_hash(WHITE))], diff, d);
    update_correction(b_non_pawn[stm][idx(board.non_pawn_hash(BLACK))], diff, d);
}

// Continuation Correction History

void ContinuationCorrectionHistory::update(const Board &board, Score eval, Score score, int d, const Stack *stack) {
    Move m = (stack - 1)->move;
    if(!m || !(stack - 2)->move)
        return;

    Piece pc = board.piece_at(m.to());
    assert(valid_piece(pc));

    update_correction((*(stack - 2)->cont_corr_hist)[pc][m.to()], (score - eval), d);
}

} // namespace search
