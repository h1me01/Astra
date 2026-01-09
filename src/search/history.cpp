#include <algorithm>
#include <cstring>

#include "history.h"
#include "tune_params.h"

namespace search {

// Helper

void update_history(int16_t &v, int bonus) {
    v += bonus - v * std::abs(bonus) / 16384;
}

void update_correction(int16_t &v, int bonus) {
    v += bonus - v * std::abs(bonus) / 1024;
}

// Quiet Histories

void QuietHistory::update(Color c, Move move, int bonus) {
    assert(valid_color(c) && move);
    update_history(data[c][move.from()][move.to()], bonus);
}

// Noisy History

void NoisyHistory::update(const Board &board, Move move, int bonus) {
    assert(move);

    Piece pc = board.piece_at(move.from());
    PieceType captured = piece_type(board.captured_piece(move));

    assert(valid_piece(pc));
    assert(valid_piece_type(captured) || move.is_prom());

    update_history(data[pc][move.to()][captured], bonus);
}

// Pawn History

void PawnHistory::update(const Board &board, Move move, int bonus) {
    assert(move);

    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));

    update_history(data[idx(board.pawn_hash())][pc][move.to()], bonus);
}

// Continuation History

void ContinuationHistory::update(Piece pc, Square to, int bonus, Stack *stack) {
    assert(valid_piece(pc) && valid_sq(to));

    for(int i : {1, 2, 4, 6})
        if((stack - i)->move)
            update_history((*(stack - i)->cont_hist)[pc][to], bonus);
}

// Correction Histories

void CorrectionHistories::update(const Board &board, int bonus) {
    Color stm = board.side_to_move();

    update_correction(pawn[stm][idx(board.pawn_hash())], bonus);
    update_correction(minor_piece[stm][idx(board.minor_piece_hash())], bonus);
    update_correction(w_non_pawn[stm][idx(board.non_pawn_hash(WHITE))], bonus);
    update_correction(b_non_pawn[stm][idx(board.non_pawn_hash(BLACK))], bonus);
}

// Continuation Correction History

void ContinuationCorrectionHistory::update(const Board &board, int bonus, const Stack *stack) {
    Move m = (stack - 1)->move;
    if(!m || !(stack - 2)->move)
        return;

    Piece pc = board.piece_at(m.to());
    assert(valid_piece(pc));

    update_correction((*(stack - 2)->cont_corr_hist)[pc][m.to()], bonus);
}

} // namespace search
