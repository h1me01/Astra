#include "../chess/movegen.h"
#include "../eval/eval.h"

#include "search.h"

namespace Search {

void Search::start(Limits limits) {
    this->limits = limits;

    Move best_move = NO_MOVE;

    // init stack
    Stack stack[MAX_PLY + 1];
    Stack *s = &stack[0];
    for(int i = 0; i < MAX_PLY; i++)
        stack[i].ply = i;

    tt.increment();

    for(root_depth = 1; root_depth <= MAX_PLY; root_depth++) {
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;

        Score score = negamax(root_depth, alpha, beta, s);

        if(is_limit_reached(root_depth))
            break;

        best_move = get_best_move();
        print_uci_info(score);
    }

    std::cout << "bestmove " << best_move << std::endl;
}

Score Search::negamax(int depth, Score alpha, Score beta, Stack *s) {
    pv_table[s->ply].length = s->ply;

    const bool root_node = s->ply == 0;
    const Score old_alpha = alpha;
    const U64 hash = board.get_hash();

    Score best_score = -VALUE_INFINITE;
    Move best_move = NO_MOVE;

    (s + 1)->killer = NO_MOVE;

    if(!root_node) {
        if(is_limit_reached(depth))
            return 0;
        if(board.is_draw(s->ply))
            return VALUE_DRAW;

        // mate distance pruning
        alpha = std::max(alpha, Score(s->ply - VALUE_MATE));
        beta = std::min(beta, Score(VALUE_MATE - s->ply - 1));
        if(alpha >= beta)
            return alpha;
    }

    if(depth == 0)
        return quiescence(alpha, beta, s);

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);
    Score tt_score = ent->get_score(s->ply);

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

    int q_count = 0, c_count = 0;

    Move q_moves[64], c_moves[64];

    for(const Move &move : legal_moves) {
        total_nodes++;

        s->move = move;

        if(move.is_cap() && c_count < 64)
            c_moves[c_count++] = move;
        else if(q_count < 64)
            q_moves[q_count++] = move;

        board.make_move(move);
        Score score = -negamax(depth - 1, -beta, -alpha, s + 1);
        board.unmake_move(move);

        if(is_limit_reached(depth))
            return 0;

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;
                update_pv(move, s->ply);
            }

            if(alpha >= beta) {
                history.update(       //
                    board,            //
                    move, s,          //
                    q_moves, q_count, //
                    c_moves, c_count, //
                    depth             //
                );

                break; // cut-off
            }
        }
    }

    // check for mate/stalemate
    if(!legal_moves.size())
        return board.in_check() ? s->ply - VALUE_MATE : VALUE_DRAW;

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
            s->ply      //
        );
    }

    return best_score;
}

Score Search::quiescence(Score alpha, Score beta, Stack *s) {
    if(is_limit_reached(0))
        return 0;

    if(board.is_draw(s->ply))
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
    Score tt_score = ent->get_score(s->ply);

    if(tt_hit && valid_score(tt_score) && valid_tt_score(tt_score, beta, ent->bound)) {
        return tt_score;
    }

    MoveList legal_moves;
    legal_moves.gen<NOISY>(board);

    for(const Move &move : legal_moves) {
        if(!board.is_legal(move))
            continue;

        total_nodes++;

        s->move = move;

        board.make_move(move);
        Score score = -quiescence(-beta, -alpha, s + 1);
        board.unmake_move(move);

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
        s->ply      //
    );

    return best_score;
}

void Search::update_pv(const Move &move, int ply) {
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
