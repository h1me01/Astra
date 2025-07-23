#include <algorithm>
#include <cstring>

#include "history.h"
#include "search.h"
#include "tune.h"

namespace Astra {
// helper

int adjusted_bonus(int value, int bonus) {
    return bonus - value * std::abs(bonus) / 16384;
}

void update_corr(int16_t &value, int diff, int depth) {
    const int bonus = std::clamp(diff * depth / 8, -256, 256);
    value = bonus - int(value) * std::abs(bonus) / 1024;
}

int history_bonus(int depth) {
    return std::min(int(max_history_bonus), history_bonus_mult * depth + history_bonus_minus);
}

int history_malus(int depth) {
    return std::min(int(max_history_malus), history_malus_mult * depth + history_malus_minus);
}

// history
void History::update(const Board &board,    //
                     Move &best, Stack *ss, //
                     Move *q_moves, int qc, //
                     Move *c_moves, int cc, //
                     int depth              //
) {
    Color stm = board.get_stm();
    int bonus = history_bonus(depth);
    int malus = history_malus(depth);

    if(best.is_quiet()) {
        Move prev_move = (ss - 1)->move;
        if(prev_move.is_valid())
            counters[prev_move.from()][prev_move.to()] = best;

        ss->killer = best;

        // idea from ethereal
        // only update quiet history if best move was important
        if(depth > 3 || qc > 1) {
            update_qh(stm, best, bonus);
            update_conth(best, ss, bonus);
            update_ph(board, best, bonus);

            // quiet maluses
            for(int i = 0; i < qc; i++) {
                Move quiet = q_moves[i];
                if(quiet == best)
                    continue;
                update_qh(stm, quiet, -malus);
                update_conth(quiet, ss, -malus);
                update_ph(board, quiet, -malus);
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
        update_nh(board, cap, -malus);
    }    
}

void History::update_qh(Color c, Move move, int bonus) {
    assert(move.is_valid());
    int16_t &value = hh[c][move.from()][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_ph(const Board &board, Move &move, int bonus) {
    assert(move.is_valid());
    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));
    int16_t &value = ph[ph_idx(board.get_pawnhash())][pc][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_conth(Move &move, Stack *ss, int bonus) {
    assert(move.is_valid());

    for(int offset : {1, 2, 4, 6}) {
        if((ss - offset)->move.is_valid()) {
            Piece pc = (ss - offset)->moved_piece;
            assert(valid_piece(pc));

            int16_t &value = (*(ss - offset)->conth)[pc][move.to()];
            value += adjusted_bonus(value, bonus);
        }
    }
}

void History::update_nh(const Board &board, Move &move, int bonus) {
    assert(move.is_valid());

    Square to = move.to();
    PieceType captured = (move.type() == EN_PASSANT) ? PAWN : piece_type(board.piece_at(to));
    Piece pc = board.piece_at(move.from());

    assert(pc != NO_PIECE);
    assert(valid_piece_type(captured) || (move.type() >= PQ_KNIGHT && move.type() <= PQ_QUEEN));

    int16_t &value = nh[pc][to][captured];
    value += adjusted_bonus(value, bonus);
}

void History::update_matcorr(const Board &board, Score raw_eval, Score real_score, int depth) {
    Color stm = board.get_stm();
    int diff = real_score - raw_eval;

    update_corr(pawn_corr[stm][corr_idx(board.get_pawnhash())], diff, depth);
    update_corr(w_non_pawn_corr[stm][corr_idx(board.get_nonpawnhash(WHITE))], diff, depth);
    update_corr(b_non_pawn_corr[stm][corr_idx(board.get_nonpawnhash(BLACK))], diff, depth);
}

void History::update_contcorr(Score raw_eval, Score real_score, int depth, const Stack *ss) {
    const Move prev_move = (ss - 1)->move;
    const Move pprev_move = (ss - 2)->move;

    Piece prev_pc = (ss - 1)->moved_piece;
    Piece pprev_pc = (ss - 2)->moved_piece;

    if(!prev_move.is_valid() || !pprev_move.is_valid())
        return;

    update_corr(cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()], real_score - raw_eval, depth);
}

} // namespace Astra
