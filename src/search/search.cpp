#include <algorithm>
#include <cmath>

#include "../chess/movegen.h"
#include "../nnue/nnue.h"

#include "movepicker.h"
#include "search.h"
#include "threads.h"

namespace Search {

int REDUCTIONS[MAX_PLY][MAX_MOVES];

void init_reductions() {
    REDUCTIONS[0][0] = 0;

    for(int depth = 1; depth < MAX_PLY; depth++)
        for(int moves = 1; moves < MAX_MOVES; moves++)
            REDUCTIONS[depth][moves] = 1.07 + log(depth) * log(moves) / 3.25;
}

// Search

void Search::start(const Board &board, Limits limits) {
    tm.start();

    if(id == 0)
        tt.increment();

    this->board = board;
    this->limits = limits;

    Move best_move = NO_MOVE;

    // init stack
    Stack stack[MAX_PLY + 7]; // +6 for continuation history, +1 for safety
    Stack *s = &stack[6];
    for(int i = 0; i < MAX_PLY + 7; i++) {
        stack[i].ply = i - 6;
        stack[i].conth = &history.conth[0][NO_PIECE][NO_SQUARE];
    }

    int stability = 0;
    Score prev_score = VALUE_NONE;
    Score scores[MAX_PLY];

    for(root_depth = 1; root_depth <= MAX_PLY; root_depth++) {
        Score score = aspiration(root_depth, prev_score, s);

        if(is_limit_reached(root_depth))
            break;

        best_move = get_best_move();
        print_uci_info(score);

        if(id == 0                                                                    //
           && root_depth >= 5                                                         //
           && tm.should_stop(                                                         //
                  limits,                                                             //
                  stability,                                                          //
                  prev_score - score,                                                 //
                  scores[root_depth - 3] - score,                                     //
                  move_nodes[best_move.from()][best_move.to()] / double(total_nodes)) //
        ) {
            break;
        }

        prev_score = score;
        scores[root_depth] = score;
    }

    if(id == 0)
        std::cout << "bestmove " << best_move << std::endl;

    threads.stop();
}

Score Search::aspiration(int depth, Score prev_score, Stack *s) {
    Score score;
    Score alpha = -VALUE_INFINITE;
    Score beta = VALUE_INFINITE;

    int delta = 11;
    if(depth >= 4) {
        alpha = std::max(prev_score - delta, int(-VALUE_INFINITE));
        beta = std::min(prev_score + delta, int(VALUE_INFINITE));
    }

    int fail_high_count = 0;
    while(true) {
        score = negamax<NodeType::ROOT>(std::max(1, root_depth - fail_high_count), alpha, beta, s);

        if(is_limit_reached(depth))
            return 0;

        if(score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(alpha - delta, int(-VALUE_INFINITE));
            fail_high_count = 0;
        } else if(score >= beta) {
            beta = std::min(beta + delta, int(VALUE_INFINITE));
            if(!is_decisive(score))
                fail_high_count++;
        } else {
            break;
        }

        delta += delta / 3;
    }

    return score;
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
        return quiescence<nt>(0, alpha, beta, s);

    depth = std::min(depth, MAX_PLY - 1);

    const Score old_alpha = alpha;
    const Color stm = board.get_stm();
    const bool in_check = board.in_check();
    const U64 hash = board.get_hash();

    Score best_score = -VALUE_INFINITE;
    Score raw_eval, eval, beta_cut;

    Move best_move = NO_MOVE;

    bool improving = false;

    (s + 1)->killer = NO_MOVE;
    (s + 1)->skipped = NO_MOVE;

    if(!root_node) {
        if(s->ply >= MAX_PLY - 1)
            return in_check ? VALUE_DRAW : evaluate();
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
    TTEntry *ent = !s->skipped.is_valid() ? tt.lookup(hash, &tt_hit) : nullptr;

    Move tt_move = NO_MOVE;
    Bound tt_bound = NO_BOUND;
    Score tt_score = VALUE_NONE;
    Score tt_eval = VALUE_NONE;
    int tt_depth = 0;
    bool tt_pv = pv_node;

    if(tt_hit) {
        tt_move = ent->get_move();
        tt_bound = ent->get_bound();
        tt_score = ent->get_score(s->ply);
        tt_eval = ent->get_eval();
        tt_depth = ent->get_depth();
        tt_pv |= ent->get_tt_pv();
    }

    if(!pv_node                                    //
       && tt_depth >= depth                        //
       && valid_score(tt_score)                    //
       && valid_tt_score(tt_score, beta, tt_bound) //
    ) {
        return tt_score;
    }

    // set eval and static eval
    if(in_check) {
        raw_eval = eval = s->static_eval = VALUE_NONE;
        goto movesloop;
    } else if(s->skipped.is_valid())
        raw_eval = eval = s->static_eval;
    else {
        raw_eval = valid_score(tt_eval) ? tt_eval : evaluate();
        eval = s->static_eval = adjust_eval(raw_eval, s);

        if(valid_score(tt_score) && valid_tt_score(tt_score, eval + 1, tt_bound)) {
            eval = tt_score;
        } else if(!tt_hit) {
            ent->store(     //
                hash,       //
                NO_MOVE,    //
                VALUE_NONE, // score
                raw_eval,   //
                NO_BOUND,   //
                0,          // depth
                s->ply,     //
                tt_pv       //
            );
        }
    }

    // update quiet history
    if((s - 1)->move.is_valid()             //
       && !(s - 1)->move.is_cap()           //
       && valid_score((s - 1)->static_eval) //
    ) {
        int bonus = std::clamp(                                 //
            -53 * (s->static_eval + (s - 1)->static_eval) / 16, //
            -72,                                                //
            286                                                 //
        );

        history.update_hh(~stm, (s - 1)->move, bonus);
    }

    if(valid_score((s - 2)->static_eval))
        improving = s->static_eval > (s - 2)->static_eval;
    else if(valid_score((s - 4)->static_eval))
        improving = s->static_eval > (s - 4)->static_eval;

    // razoring
    if(!pv_node                      //
       && !is_win(alpha)             //
       && depth < 5                  //
       && eval + 258 * depth < alpha //
    ) {
        Score score = quiescence<NodeType::NON_PV>(0, alpha, beta, s);
        if(score <= alpha)
            return score;
    }

    // reverse futility pruning
    if(!pv_node                                         //
       && depth < 11                                    //
       && !is_win(eval)                                 //
       && !is_loss(beta)                                //
       && !s->skipped.is_valid()                        //
       && eval - (106 * depth - 89 * improving) >= beta //
    ) {
        return (eval + beta) / 2;
    }

    // null move pruning
    if(!pv_node                                     //
       && depth >= 4                                //
       && eval >= beta                              //
       && !is_loss(beta)                            //
       && board.nonpawnmat(stm)                     //
       && !s->skipped.is_valid()                    //
       && (s - 1)->move != NULL_MOVE                //
       && s->static_eval + 25 * depth - 161 >= beta //
    ) {
        int R = 4           //
                + depth / 3 //
                + std::min(4, (eval - beta) / 217);

        s->move = NULL_MOVE;
        s->moved_piece = NO_PIECE;
        s->conth = &history.conth[0][NO_PIECE][NO_SQUARE];

        board.make_nullmove();
        Score score = -negamax<NodeType::NON_PV>(depth - R, -beta, -beta + 1, s + 1);
        board.unmake_nullmove();

        if(score >= beta) {
            // don't return unproven mate scores
            if(is_win(score))
                score = beta;
            return score;
        }
    }

    // probcut
    beta_cut = beta + 237;
    if(!pv_node                                           //
       && depth > 4                                       //
       && !is_decisive(beta)                              //
       && !s->skipped.is_valid()                          //
       && !(tt_depth >= depth - 3 && tt_score < beta_cut) //
    ) {
        MovePicker mp(PC_SEARCH, board, history, s, tt_move);
        mp.see_cutoff = beta_cut > s->static_eval;

        Move move = NO_MOVE;
        while((move = mp.next()) != NO_MOVE) {
            if(!board.is_legal(move))
                continue;

            tt.prefetch(board.key_after(move));

            total_nodes++;
            s->move = move;
            s->moved_piece = board.piece_at(move.from());
            s->conth = &history.conth[move.is_cap()][s->moved_piece][move.to()];

            board.make_move(move);
            Score score = -quiescence<NodeType::NON_PV>(0, -beta_cut, -beta_cut + 1, s + 1);

            if(score >= beta_cut)
                score = -negamax<NodeType::NON_PV>(depth - 4, -beta_cut, -beta_cut + 1, s + 1);

            board.unmake_move(move);

            if(score >= beta_cut) {
                ent->store(      //
                    hash,        //
                    move,        //
                    score,       //
                    raw_eval,    //
                    LOWER_BOUND, //
                    depth - 3,   //
                    s->ply,      //
                    tt_pv        //
                );

                return score;
            }
        }
    }

movesloop:

    MovePicker mp(N_SEARCH, board, history, s, tt_move);
    Move move = NO_MOVE;

    int made_moves = 0, q_count = 0, c_count = 0;

    Move q_moves[64], c_moves[64];

    while((move = mp.next()) != NO_MOVE) {
        if(move == s->skipped || !board.is_legal(move))
            continue;

        tt.prefetch(board.key_after(move));

        U64 start_nodes = total_nodes;

        made_moves++;

        int history_score = move.is_quiet() ? history.get_qh(board, s, move) //
                                            : history.get_nh(board, move);

        if(!root_node && !is_loss(best_score)) {
            const int lmr_depth = std::max(0, depth - REDUCTIONS[depth][made_moves] + history_score / 7848);

            // late move pruning
            if(q_count > (3 + depth * depth) / (2 - improving))
                mp.skip_quiets();

            // futility pruning
            if(!move.is_cap()                                    //
               && lmr_depth < 9                                  //
               && s->static_eval + 98 + lmr_depth * 107 <= alpha //
            ) {
                mp.skip_quiets();
            }

            // history pruning
            if(!move.is_cap() && lmr_depth < 5 && history_score < -6775 * depth) {
                mp.skip_quiets();
                continue;
            }

            // see pruning
            int see_depth = move.is_cap() ? depth : lmr_depth * lmr_depth;
            int see_margin = move.is_cap() ? 97 : 17;
            if(!board.see(move, see_depth * -see_margin))
                continue;
        }

        s->move = move;
        s->moved_piece = board.piece_at(move.from());
        s->conth = &history.conth[move.is_cap()][s->moved_piece][move.to()];

        if(move.is_cap() && c_count < 64)
            c_moves[c_count++] = move;
        else if(q_count < 64)
            q_moves[q_count++] = move;

        // singular extensions
        int extensions = 0;

        if(!root_node                 //
           && depth >= 6              //
           && move == tt_move         //
           && !s->skipped.is_valid()  //
           && tt_depth >= depth - 3   //
           && valid_score(tt_score)   //
           && !is_decisive(tt_score)  //
           && tt_bound & LOWER_BOUND  //
           && s->ply < 2 * root_depth //
        ) {
            Score sbeta = tt_score - 6 * depth / 8;
            s->skipped = move;
            Score score = negamax<NodeType::NON_PV>((depth - 1) / 2, sbeta - 1, sbeta, s);
            s->skipped = NO_MOVE;

            if(score < sbeta) {
                extensions = 1;
                if(!pv_node && score < sbeta - 14)
                    extensions = 2 + (move.is_quiet() && score < sbeta - 74);
            } else if(sbeta >= beta)
                return sbeta;
            else if(tt_score >= beta)
                extensions = -3;
        }

        total_nodes++;

        int new_depth = depth - 1 + extensions;

        board.make_move(move);

        Score score = VALUE_NONE;

        // late move reductions
        if(depth >= 2 && made_moves >= 3 && (!pv_node || !move.is_cap())) {
            int r = REDUCTIONS[depth][made_moves];

            r += !improving;

            r += tt_move.is_valid() ? tt_move.is_cap() : 0;

            r -= tt_pv;

            r -= board.in_check();

            r -= tt_depth >= depth;

            r -= history_score / (move.is_cap() ? 3886 : 7489);

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

        if(root_node) {
            move_nodes[move.from()][move.to()] += total_nodes - start_nodes;
        }

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
        if(s->skipped.is_valid())
            return alpha;
        else if(in_check)
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

    if(!s->skipped.is_valid()) {
        ent->store(     //
            hash,       //
            best_move,  //
            best_score, //
            raw_eval,   //
            bound,      //
            depth,      //
            s->ply,     //
            tt_pv       //
        );
    }

    // update correction histories
    if(!in_check                                            //
       && (!best_move.is_valid() || !best_move.is_cap())    //
       && valid_tt_score(best_score, s->static_eval, bound) //
    ) {
        history.update_matcorr(board, raw_eval, best_score, depth);
        history.update_contcorr(raw_eval, best_score, depth, s);
    }

    assert(valid_score(best_score));

    return best_score;
}

template <NodeType nt> //
Score Search::quiescence(int depth, Score alpha, Score beta, Stack *s) {

    constexpr bool pv_node = (nt != NodeType::NON_PV);

    assert(s->ply >= 0);
    assert(pv_node || (alpha == beta - 1));
    assert(valid_score(alpha + 1) && valid_score(beta - 1) && alpha < beta);

    if(is_limit_reached(0))
        return 0;

    if(board.is_draw(s->ply))
        return VALUE_DRAW;

    const bool in_check = board.in_check();
    const U64 hash = board.get_hash();

    if(s->ply >= MAX_PLY - 1)
        return in_check ? VALUE_DRAW : evaluate();

    Move best_move = NO_MOVE;

    Score best_score = -VALUE_INFINITE;
    Score raw_eval, futility;

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);

    Move tt_move = NO_MOVE;
    Bound tt_bound = NO_BOUND;
    Score tt_score = VALUE_NONE;
    Score tt_eval = VALUE_NONE;
    bool tt_pv = false;

    if(tt_hit) {
        tt_move = ent->get_move();
        tt_bound = ent->get_bound();
        tt_score = ent->get_score(s->ply);
        tt_eval = ent->get_eval();
        tt_pv |= ent->get_tt_pv();
    }

    if(!pv_node && valid_score(tt_score) && valid_tt_score(tt_score, beta, tt_bound)) {
        return tt_score;
    }

    if(in_check) {
        futility = raw_eval = s->static_eval = VALUE_NONE;
    } else {
        raw_eval = valid_score(tt_eval) ? tt_eval : evaluate();
        best_score = s->static_eval = adjust_eval(raw_eval, s);
        futility = best_score + 114;

        if(valid_score(tt_score) && valid_tt_score(tt_score, best_score + 1, tt_bound)) {
            best_score = tt_score;
        }

        // stand pat
        if(best_score >= beta) {
            if(!tt_hit) {
                ent->store(     //
                    hash,       //
                    NO_MOVE,    //
                    VALUE_NONE, // score
                    raw_eval,   //
                    NO_BOUND,   //
                    0,          // depth
                    s->ply,     //
                    false       // tt_pv
                );
            }

            return best_score;
        }

        if(best_score > alpha)
            alpha = best_score;
    }

    MovePicker mp(Q_SEARCH, board, history, s, tt_move, depth >= -1);
    Move move = NO_MOVE;

    int made_moves = 0;
    while((move = mp.next()) != NO_MOVE) {
        if(!board.is_legal(move))
            continue;

        tt.prefetch(board.key_after(move));

        made_moves++;

        if(!is_loss(best_score)) {
            if(!in_check && !move.is_quiet() && futility <= alpha && !board.see(move, 1)) {
                best_score = std::max(best_score, futility);
                continue;
            }

            if(!board.see(move, 0))
                continue;
        }

        total_nodes++;

        s->move = move;
        s->moved_piece = board.piece_at(move.from());
        s->conth = &history.conth[move.is_cap()][s->moved_piece][move.to()];

        board.make_move(move);
        Score score = -quiescence<nt>(depth - 1, -beta, -alpha, s + 1);
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
        raw_eval,   //
        bound,      //
        0,          // depth
        s->ply,     //
        tt_pv       //
    );

    assert(valid_score(best_score));

    return best_score;
}

Score Search::evaluate() {
    int eval = NNUE::nnue.forward(board);
    return std::clamp(eval, int(-VALUE_MATE_IN_MAX_PLY), int(VALUE_MATE_IN_MAX_PLY));
}

Score Search::adjust_eval(Score eval, Stack *s) const {
    eval += (history.get_matcorr(board) + history.get_contcorr(s)) / 256;

    return std::clamp(                       //
        eval,                                //
        Score(VALUE_TB_LOSS_IN_MAX_PLY + 1), //
        Score(VALUE_TB_WIN_IN_MAX_PLY - 1)   //
    );
}

void Search::update_pv(const Move &move, int ply) {
    pv_table[ply][ply] = move;
    for(int nply = ply + 1; nply < pv_table[ply + 1].length; nply++)
        pv_table[ply][nply] = pv_table[ply + 1][nply];
    pv_table[ply].length = pv_table[ply + 1].length;
}

void Search::print_uci_info(Score score) const {
    if(id != 0)
        return; // only main thread prints UCI info

    const int64_t elapsed_time = tm.elapsed_time();
    const U64 total_thread_nodes = threads.get_total_nodes();

    const PVLine &pv_line = pv_table[0];

    std::cout << "info depth " << root_depth //
              << " score ";                  //

    if(std::abs(score) >= VALUE_MATE_IN_MAX_PLY)
        std::cout << "mate " << (VALUE_MATE - std::abs(score) + 1) / 2 * (score > 0 ? 1 : -1);
    else
        std::cout << "cp " << Score(score / 2.5);

    std::cout << " nodes " << total_thread_nodes                           //
              << " nps " << total_thread_nodes * 1000 / (elapsed_time + 1) //
              << " hashfull " << tt.hashfull()                             //
              << " time " << elapsed_time                                  //
              << " pv " << pv_line[0];

    // print rest of the pv
    for(int i = 1; i < pv_line.length; i++)
        std::cout << " " << pv_line[i];

    std::cout << std::endl;
}

bool Search::is_limit_reached(int depth) const {
    if(threads.is_stopped())
        return true;
    if(id != 0)
        return false; // only main thread checks limits

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
