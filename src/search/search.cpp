#include "../chess/movegen.h"
#include "../eval/eval.h"

#include "search.h"

namespace Search {

void Search::start() {
    for(root_depth = 1; root_depth <= MAX_PLY; root_depth++) {
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;

        Score score = negamax(root_depth, alpha, beta);

        std::cout << "Depth: " << root_depth << ", Score: " << score << ", PV: " << best_move();
        for(int i = 1; i < pv_table[0].length; i++) {
            std::cout << " " << pv_table[0][i];
        }
        std::cout << std::endl;
    }
}

Score Search::negamax(int depth, Score alpha, Score beta) {
    pv_table[ply].length = ply;

    if(depth == 0)
        return Eval::evaluate(board);

    Score best_score = -VALUE_INFINITE;

    MoveList legal_moves;
    legal_moves.gen<LEGALS>(board);

    for(const Move &move : legal_moves) {
        make_move(move);
        Score score = -negamax(depth - 1, -beta, -alpha);
        unmake_move(move);

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                update_pv(move);
            }

            if(alpha >= beta)
                break; // cut-off
        }
    }

    // check for mate/stalemate
    if(!legal_moves.size())
        return board.in_check() ? ply - VALUE_MATE : VALUE_DRAW;

    return best_score;
}

void Search::update_pv(const Move &move) {
    pv_table[ply][ply] = move;
    for(int nply = ply + 1; nply < pv_table[ply + 1].length; nply++)
        pv_table[ply][nply] = pv_table[ply + 1][nply];
    pv_table[ply].length = pv_table[ply + 1].length;
}

} // namespace Search
