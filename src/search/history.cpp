#include "history.h"

namespace Search {

int history_bonus(int depth) {
    return std::min(2000, 270 * depth - 30);
}

int adjusted_bonus(int value, int bonus) {
    return bonus - value * std::abs(bonus) / 16384;
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
        if(prev_move.is_valid())
            counters[prev_move.from()][prev_move.to()] = best;

        s->killer = best;

        // idea from ethereal
        // only update quiet history if best move was important
        if(depth > 3 || qc > 1) {
            update_hh(stm, best, bonus);

            // quiet maluses
            for(int i = 0; i < qc; i++) {
                Move quiet = q_moves[i];
                if(quiet == best)
                    continue;
                update_hh(stm, quiet, -bonus);
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
    assert(move.is_valid());
    int16_t &value = hh[c][move.from()][move.to()];
    value += adjusted_bonus(value, bonus);
}

void History::update_nh(const Board &board, const Move &move, int bonus) {
    assert(move.is_valid());

    Square to = move.to();
    PieceType captured = (move.type() == EN_PASSANT) ? PAWN : piece_type(board.piece_at(to));
    Piece pc = board.piece_at(move.from());

    assert(pc != NO_PIECE);
    assert(valid_piece_type(captured) || (move.type() >= PQ_KNIGHT && move.type() <= PQ_QUEEN));

    int16_t &value = nh[pc][to][captured];
    value += adjusted_bonus(value, bonus);
}

} // namespace Search
