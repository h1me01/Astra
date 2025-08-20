#include "../chess/movegen.h"
#include "../eval/eval.h"

#include "search.h"

namespace Search {

void Search::start(Limits limits) {
    this->limits = limits;
    ply = 0;

    Move best_move = NO_MOVE;

    tt.increment();

    for(root_depth = 1; root_depth <= MAX_PLY; root_depth++) {
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;

        Score score = negamax(root_depth, alpha, beta);

        if(is_limit_reached(root_depth))
            break;

        best_move = get_best_move();
        print_uci_info(score);
    }

    std::cout << "bestmove " << best_move << std::endl;
}

Score Search::negamax(int depth, Score alpha, Score beta) {
    pv_table[ply].length = ply;

    const bool root_node = ply == 0;
    const Score old_alpha = alpha;
    const U64 hash = board.get_hash();

    Score best_score = -VALUE_INFINITE;
    Move best_move = NO_MOVE;

    if(!root_node) {
        if(is_limit_reached(depth))
            return 0;
        if(board.is_draw(ply))
            return VALUE_DRAW;

        // mate distance pruning
        alpha = std::max(alpha, Score(ply - VALUE_MATE));
        beta = std::min(beta, Score(VALUE_MATE - ply - 1));
        if(alpha >= beta)
            return alpha;
    }

    if(depth == 0)
        return quiescence(alpha, beta);

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);
    Score tt_score = ent->get_score(ply);

    if(!root_node                                    //
       && tt_hit                                     //
       && ent->depth >= depth                        //
       && valid_score(tt_score)                      //
       && valid_tt_score(tt_score, beta, ent->bound) //
    ) {
        return tt_score;
    }

    MoveList legal_moves;
    legal_moves.gen<LEGALS>(board);

    for(const Move &move : legal_moves) {
        total_nodes++;

        make_move(move);
        Score score = -negamax(depth - 1, -beta, -alpha);
        unmake_move(move);

        if(is_limit_reached(depth))
            return 0;

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;
                update_pv(move);
            }

            if(alpha >= beta)
                break; // cut-off
        }
    }

    // check for mate/stalemate
    if(!legal_moves.size())
        return board.in_check() ? ply - VALUE_MATE : VALUE_DRAW;

    // store in transposition table
    Bound bound = EXACT_BOUND;
    if(best_score >= beta)
        bound = LOWER_BOUND;
    else if(best_score <= old_alpha)
        bound = UPPER_BOUND;

    if(best_move.is_valid()) {
        ent->store(     //
            hash,       //
            best_move,  //
            best_score, //
            bound,      //
            depth,      //
            ply         //
        );
    }

    return best_score;
}

Score Search::quiescence(Score alpha, Score beta) {
    if(is_limit_reached(0))
        return 0;

    if(board.is_draw(ply))
        return VALUE_DRAW;

    const U64 hash = board.get_hash();
    Move best_move = NO_MOVE;

    Score best_score = Eval::evaluate(board);
    if(best_score >= beta)
        return best_score;
    if(best_score > alpha)
        alpha = best_score;

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);
    Score tt_score = ent->get_score(ply);

    if(tt_hit && valid_score(tt_score) && valid_tt_score(tt_score, beta, ent->bound)) {
        return tt_score;
    }

    MoveList legal_moves;
    legal_moves.gen<NOISY>(board);

    for(const Move &move : legal_moves) {
        if(!board.is_legal(move))
            continue;

        total_nodes++;

        make_move(move);
        Score score = -quiescence(-beta, -alpha);
        unmake_move(move);

        if(is_limit_reached(0))
            return 0;

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;
            }

            if(alpha >= beta)
                break; // cut-off
        }
    }

    Bound bound = (best_score >= beta) ? LOWER_BOUND : UPPER_BOUND;

    ent->store(     //
        hash,       //
        best_move,  //
        best_score, //
        bound,      //
        0,          // depth
        ply         //
    );

    return best_score;
}

void Search::update_pv(const Move &move) {
    pv_table[ply][ply] = move;
    for(int nply = ply + 1; nply < pv_table[ply + 1].length; nply++)
        pv_table[ply][nply] = pv_table[ply + 1][nply];
    pv_table[ply].length = pv_table[ply + 1].length;
}

void Search::print_uci_info(Score score) const {
    const int64_t elapsed_time = tm.elapsed_time();
    const PVLine &pv_line = pv_table[0];

    std::cout << "info depth " << root_depth //
              << " score ";                  //

    if(std::abs(score) >= VALUE_MATE_IN_MAX_PLY)
        std::cout << "mate " << (VALUE_MATE - std::abs(score) + 1) / 2 * (score > 0 ? 1 : -1);
    else
        std::cout << "cp " << score;

    std::cout << " nodes " << total_nodes                           //
              << " nps " << total_nodes * 1000 / (elapsed_time + 1) //
              << " hashfull " << tt.hashfull()                      //
              << " time " << elapsed_time                           //
              << " pv " << pv_line[0];

    // print rest of the pv
    for(int i = 1; i < pv_line.length; i++)
        std::cout << " " << pv_line[i];

    std::cout << std::endl;
}

bool Search::is_limit_reached(int depth) const {
    if(limits.infinite)
        return false;
    if(depth > limits.depth)
        return true;
    if(limits.nodes != 0 && total_nodes >= limits.nodes)
        return true;
    if(limits.time.maximum != 0 && tm.elapsed_time() >= limits.time.maximum)
        return true;

    return false;
}

} // namespace Search
