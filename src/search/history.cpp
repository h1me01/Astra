#include <algorithm>

#include "history.h"

namespace Search {

int history_bonus(int depth) {
    return std::min(2000, 270 * depth - 30);
}

int adjusted_bonus(int value, int bonus) {
    return bonus - value * std::abs(bonus) / 16384;
}

void update_corr(int16_t &value, int diff, int depth) {
    const int bonus = std::clamp(diff * depth / 8, -256, 256);
    value = bonus - int(value) * std::abs(bonus) / 1024;
}

// History

void History::update(const Board &board,         //
                     const Move &best, Stack *s, //
                     Move *q_moves, int qc,      //
                     Move *c_moves, int cc,      //
                     int depth                   //
) {
    Color stm = board.get_stm();
    int bonus = history_bonus(depth);

    if(best.is_quiet()) {
        Move prev_move = (s - 1)->move;
        if(prev_move)
            counters[prev_move.from()][prev_move.to()] = best;

        s->killer = best;

        // idea from ethereal
        // only update quiet history if best move was important
        if(depth > 3 || qc > 1) {
            update_hh(stm, best, bonus);
            update_ph(board, best, bonus);
            update_conth(best, s, bonus);

            // quiet maluses
            for(int i = 0; i < qc; i++) {
                Move quiet = q_moves[i];
                if(quiet == best)
                    continue;
                update_hh(stm, quiet, -bonus);
                update_conth(quiet, s, -bonus);
                update_ph(board, quiet, -bonus);
            }
        }
    } else {
        update_nh(board, best, bonus);
    }

    // capture maluses
    for(int i = 0; i < cc; i++) {
        Move cap = c_moves[i];
        if(cap == best)
            continue;
        update_nh(board, cap, -bonus);
    }
}

void History::update_hh(Color c, const Move &move, int bonus) {
    assert(move);
    int16_t &value = hh[c][move.from()][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_nh(const Board &board, const Move &move, int bonus) {
    assert(move);

    Square to = move.to();
    PieceType captured = (move.type() == EN_PASSANT) ? PAWN : piece_type(board.piece_at(to));
    Piece pc = board.piece_at(move.from());

    assert(pc != NO_PIECE);
    assert(valid_piece_type(captured) || (move.type() >= PQ_KNIGHT && move.type() <= PQ_QUEEN));

    int16_t &value = nh[pc][to][captured];
    value += adjusted_bonus(value, bonus);
}

void History::update_ph(const Board &board, const Move &move, int bonus) {
    assert(move);
    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));
    int16_t &value = ph[ph_idx(board.get_pawn_hash())][pc][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_conth(const Move &move, Stack *s, int bonus) {
    assert(move);

    for(int offset : {1, 2, 4, 6}) {
        if((s - offset)->move) {
            Piece pc = (s - offset)->moved_piece;
            assert(valid_piece(pc));

            int16_t &value = (*(s - offset)->conth)[pc][move.to()];
            value += adjusted_bonus(value, bonus);
        }
    }
}

void History::update_matcorr(const Board &board, Score raw_eval, Score real_score, int depth) {
    Color stm = board.get_stm();
    int diff = real_score - raw_eval;

    update_corr(pawn_corr[stm][corr_idx(board.get_pawn_hash())], diff, depth);
    update_corr(w_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(WHITE))], diff, depth);
    update_corr(b_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(BLACK))], diff, depth);
}

void History::update_contcorr(Score raw_eval, Score real_score, int depth, const Stack *s) {
    const Move prev_move = (s - 1)->move;
    const Move pprev_move = (s - 2)->move;

    Piece prev_pc = (s - 1)->moved_piece;
    Piece pprev_pc = (s - 2)->moved_piece;

    if(!prev_move || !pprev_move)
        return;

    update_corr(cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()], real_score - raw_eval, depth);
}

} // namespace Search
