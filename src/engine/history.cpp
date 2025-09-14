#include <algorithm>
#include <cstring>

#include "history.h"
#include "tune_params.h"

namespace Engine {

int history_bonus(int depth) {
    return std::min(int(max_history_bonus), history_bonus_mult * depth + history_bonus_minus);
}

int history_malus(int depth) {
    return std::min(int(max_history_malus), history_malus_mult * depth + history_malus_minus);
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
    std::memset(conth, 0, sizeof(conth));

    std::fill(&counters[0][0], &counters[0][0] + NUM_SQUARES * NUM_SQUARES, Move{});

    std::memset(hh, 0, sizeof(hh));
    std::memset(nh, 0, sizeof(nh));
    std::memset(ph, 0, sizeof(ph));

    std::memset(pawn_corr, 0, sizeof(pawn_corr));
    std::memset(w_non_pawn_corr, 0, sizeof(w_non_pawn_corr));
    std::memset(b_non_pawn_corr, 0, sizeof(b_non_pawn_corr));

    std::memset(cont_corr, 0, sizeof(cont_corr));
}

void History::update(         //
    const Board &board,       //
    const Move &best_move,    //
    const MoveList<> &quiets, //
    const MoveList<> &noisy,  //
    Stack *stack,             //
    int depth                 //
) {
    Color stm = board.get_stm();

    int bonus = history_bonus(depth);
    int malus = history_malus(depth);

    if(best_move.is_quiet()) {
        Move prev_move = (stack - 1)->move;
        if(prev_move)
            counters[prev_move.from()][prev_move.to()] = best_move;

        stack->killer = best_move;

        // idea from ethereal
        // only update quiet history if best move was important
        if(depth > 3 || quiets.size() > 1) {
            update_hh(stm, best_move, bonus);
            update_ph(board, best_move, bonus);
            update_conth(best_move, stack, bonus);

            // quiet maluses
            for(const auto &m : quiets) {
                update_hh(stm, m, -malus);
                update_conth(m, stack, -malus);
                update_ph(board, m, -malus);
            }
        }
    } else {
        update_nh(board, best_move, bonus);
    }

    // noisy maluses
    for(const auto &m : noisy)
        update_nh(board, m, -malus);
}

void History::update_hh(Color c, const Move &move, int bonus) {
    assert(move);
    int16_t &value = hh[c][move.from()][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_nh(const Board &board, const Move &move, int bonus) {
    assert(move);

    Square to = move.to();
    PieceType captured = move.is_ep() ? PAWN : piece_type(board.piece_at(to));
    Piece pc = board.piece_at(move.from());

    assert(valid_piece(pc));
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

void History::update_conth(const Move &move, Stack *stack, int bonus) {
    assert(move);

    for(int i : {1, 2, 4, 6}) {
        if((stack - i)->move) {
            Piece pc = (stack - i)->moved_piece;
            assert(valid_piece(pc));

            int16_t &value = (*(stack - i)->conth)[pc][move.to()];
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

void History::update_contcorr(Score raw_eval, Score real_score, int depth, const Stack *stack) {
    const Move prev_move = (stack - 1)->move;
    const Move pprev_move = (stack - 2)->move;

    Piece prev_pc = (stack - 1)->moved_piece;
    Piece pprev_pc = (stack - 2)->moved_piece;

    if(!prev_move || !pprev_move)
        return;

    update_corr(cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()], real_score - raw_eval, depth);
}

} // namespace Engine
