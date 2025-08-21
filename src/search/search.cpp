#include <algorithm>
#include <cmath>

#include "../chess/movegen.h"
#include "../nnue/nnue.h"

#include "movepicker.h"
#include "search.h"

namespace Search {

int REDUCTIONS[MAX_PLY][MAX_MOVES];

void init_reductions() {
    REDUCTIONS[0][0] = 0;

    for(int depth = 1; depth < MAX_PLY; depth++)
        for(int moves = 1; moves < MAX_MOVES; moves++)
            REDUCTIONS[depth][moves] = 1 + log(depth) * log(moves) / 1.75;
}

// Search

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

        Score score = negamax<NodeType::ROOT>(root_depth, alpha, beta, s);

        if(is_limit_reached(root_depth))
            break;

        best_move = get_best_move();
        print_uci_info(score);
    }

    std::cout << "bestmove " << best_move << std::endl;
}

template <NodeType nt> //
Score Search::negamax(int depth, Score alpha, Score beta, Stack *s) {

    constexpr bool root_node = (nt == NodeType::ROOT);
    constexpr bool pv_node = (nt != NodeType::NON_PV);

    assert(s->ply >= 0);
    assert(pv_node || (alpha == beta - 1));
    assert(valid_score(alpha + 1) && valid_score(beta - 1) && alpha < beta);

    if(is_limit_reached(depth))
        return 0;

    if(pv_node)
        pv_table[s->ply].length = s->ply;

    if(depth <= 0)
        return quiescence<nt>(alpha, beta, s);

    const Score old_alpha = alpha;
    const U64 hash = board.get_hash();

    Score best_score = -VALUE_INFINITE;
    Move best_move = NO_MOVE;

    (s + 1)->killer = NO_MOVE;

    if(!root_node) {
        if(board.is_draw(s->ply))
            return VALUE_DRAW;

        // mate distance pruning
        alpha = std::max(alpha, Score(s->ply - VALUE_MATE));
        beta = std::min(beta, Score(VALUE_MATE - s->ply - 1));
        if(alpha >= beta)
            return alpha;
    }

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);
    Score tt_score = ent->get_score(s->ply);

    if(!pv_node                                      //
       && tt_hit                                     //
       && ent->depth >= depth                        //
       && valid_score(tt_score)                      //
       && valid_tt_score(tt_score, beta, ent->bound) //
    ) {
        return tt_score;
    }

    MovePicker mp(N_SEARCH, board, history, s, ent->move);
    Move move = NO_MOVE;

    int made_moves = 0, q_count = 0, c_count = 0;

    Move q_moves[64], c_moves[64];

    while((move = mp.next()) != NO_MOVE) {
        if(!board.is_legal(move))
            continue;

        tt.prefetch(board.key_after(move));

        made_moves++;
        total_nodes++;

        s->move = move;

        if(move.is_cap() && c_count < 64)
            c_moves[c_count++] = move;
        else if(q_count < 64)
            q_moves[q_count++] = move;

        int new_depth = depth - 1;

        board.make_move(move);

        Score score = VALUE_NONE;

        // late move reductions
        if(depth >= 2 && made_moves >= 3 && (!pv_node || !move.is_cap())) {
            int r = REDUCTIONS[depth][made_moves];

            r -= move.is_cap();

            r -= pv_node;

            r -= board.in_check();

            const int lmr_depth = std::clamp(new_depth - r, 1, new_depth + 1);

            score = -negamax<NodeType::NON_PV>(lmr_depth, -alpha - 1, -alpha, s + 1);

            if(score > alpha && lmr_depth < new_depth)
                score = -negamax<NodeType::NON_PV>(new_depth, -alpha - 1, -alpha, s + 1);
        } else if(!pv_node || made_moves > 1) {
            score = -negamax<NodeType::NON_PV>(new_depth, -alpha - 1, -alpha, s + 1);
        }

        // principal variation search
        if(pv_node && (score > alpha || made_moves == 1))
            score = -negamax<NodeType::PV>(new_depth, -beta, -alpha, s + 1);

        board.unmake_move(move);

        assert(valid_score(score));

        if(is_limit_reached(depth))
            return 0;

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;

                if(pv_node)
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
    if(made_moves == 0) {
        if(board.in_check())
            return s->ply - VALUE_MATE;
        else
            return VALUE_DRAW;
    }

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

    assert(valid_score(best_score));

    return best_score;
}

template <NodeType nt> //
Score Search::quiescence(Score alpha, Score beta, Stack *s) {

    constexpr bool pv_node = (nt != NodeType::NON_PV);

    assert(s->ply >= 0);
    assert(pv_node || (alpha == beta - 1));
    assert(valid_score(alpha + 1) && valid_score(beta - 1) && alpha < beta);

    if(is_limit_reached(0))
        return 0;

    if(board.is_draw(s->ply))
        return VALUE_DRAW;

    const U64 hash = board.get_hash();
    Move best_move = NO_MOVE;

    Score best_score = evaluate();
    if(best_score >= beta)
        return best_score;
    if(best_score > alpha)
        alpha = best_score;

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);
    Score tt_score = ent->get_score(s->ply);

    if(!pv_node                                      //
       && tt_hit                                     //
       && valid_score(tt_score)                      //
       && valid_tt_score(tt_score, beta, ent->bound) //
    ) {
        return tt_score;
    }

    MovePicker mp(Q_SEARCH, board, history, s, ent->move);
    Move move = NO_MOVE;

    int made_moves = 0;
    while((move = mp.next()) != NO_MOVE) {
        if(!board.is_legal(move))
            continue;

        tt.prefetch(board.key_after(move));

        made_moves++;
        total_nodes++;

        s->move = move;

        board.make_move(move);
        Score score = -quiescence<nt>(-beta, -alpha, s + 1);
        board.unmake_move(move);

        assert(valid_score(score));

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

        // if we are not losing after being in check, it is safe to skip all quiet moves
        if(!is_loss(best_score))
            mp.skip_quiets();
    }

    if(board.in_check() && made_moves == 0)
        return s->ply - VALUE_MATE;

    Bound bound = (best_score >= beta) ? LOWER_BOUND : UPPER_BOUND;

    ent->store(     //
        hash,       //
        best_move,  //
        best_score, //
        bound,      //
        0,          // depth
        s->ply      //
    );

    assert(valid_score(best_score));

    return best_score;
}

Score Search::evaluate() const {
    int eval = NNUE::nnue.forward(board);
    return std::clamp(eval, int(-VALUE_MATE_IN_MAX_PLY), int(VALUE_MATE_IN_MAX_PLY));
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
        std::cout << "cp " << Score(score / 2.5);

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
