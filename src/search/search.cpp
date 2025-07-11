#include <cmath>

#include "../net/nnue.h"
#include "../uci.h"
#include "movepicker.h"
#include "search.h"
#include "syzygy.h"
#include "threads.h"
#include "tune.h"

namespace Astra {
int REDUCTIONS[MAX_PLY][MAX_MOVES];

void init_reductions() {
    REDUCTIONS[0][0] = 0;

    const double base = double(lmr_base) / 100.0;
    const double div_factor = double(lmr_div) / 100.0;

    for(int depth = 1; depth < MAX_PLY; depth++)
        for(int moves = 1; moves < MAX_MOVES; moves++)
            REDUCTIONS[depth][moves] = base + log(depth) * log(moves) / div_factor;
}

// search class

Search::Search(const std::string &fen) : board(fen), nodes(0), tb_hits(0) {}

Move Search::bestmove() {
    tm.start();

    MoveList<> legals;
    legals.gen<LEGALS>(board);

    if(legals.size() == 0) {
        std::cout << "Position has no legal moves" << std::endl;
        return NO_MOVE;
    }

    for(int i = 0; i < legals.size(); i++)
        rootmoves.add({legals[i], 0, 0, 0, {}});

    const int multipv_size = std::min(limit.multipv, rootmoves.size());

    if(use_tb) {
        const auto dtz = probe_dtz(board);
        if(dtz.second != NO_MOVE) {
            if(id == 0)
                std::cout << "bestmove " << dtz.second << std::endl;
            return dtz.second;
        }
    }

    if(id == 0)
        tt.increment();

    // init stack
    Stack stack[MAX_PLY + 6]; // +6 for history
    Stack *ss = &stack[6];
    for(int i = 0; i < MAX_PLY + 6; i++) {
        stack[i].ply = i - 6;
        stack[i].conth = &history.conth[0][WHITE_PAWN][a1];
    }

    int stability = 0;
    Score prev_result = VALUE_NONE;
    Score scores[MAX_PLY];

    Move bestmove = NO_MOVE, prev_bestmove = NO_MOVE;
    for(root_depth = 1; root_depth < MAX_PLY; root_depth++) {
        for(multipv_idx = 0; multipv_idx < multipv_size; multipv_idx++)
            aspiration(root_depth, ss);

        if(is_limit_reached(root_depth))
            break;

        // print final pv info if main thread
        for(multipv_idx = 0; multipv_idx < multipv_size && id == 0; multipv_idx++)
            print_uci_info();

        Score result = rootmoves[0].score;
        bestmove = rootmoves[0].move;

        if(id == 0 && root_depth >= 5 && limit.time.optimum) {
            int64_t elapsed = tm.elapsed_time();

            // adjust time optimum based on stability
            stability = bestmove == prev_bestmove ? std::min(9, stability + 1) : 0;
            double stability_factor = (stability_base / 100.0) - stability * (stability_mult / 1000.0);

            // adjust time optimum based on last score
            double result_change_factor = (results_base / 100.0)                              //
                                          + (results_mult1 / 1000.0) * (prev_result - result) //
                                          + (results_mult2 / 1000.0) * (scores[root_depth - 3] - result);

            result_change_factor = std::clamp(result_change_factor, results_min / 100.0, results_max / 100.0);

            // adjust time optimum based on node count
            double not_best_nodes = 1.0 - double(rootmoves[0].nodes / double(nodes));
            double node_count_factor = not_best_nodes * (node_mult / 100.0) + (node_base / 100.0);

            // check if we should stop
            if(elapsed > limit.time.optimum * stability_factor * result_change_factor * node_count_factor)
                break;
        }

        prev_bestmove = bestmove;
        prev_result = result;
        scores[root_depth] = result;
    }

    if(id == 0)
        std::cout << "bestmove " << bestmove << std::endl;

    // stop all threads
    threads.stop();

    return bestmove;
}

Score Search::aspiration(int depth, Stack *ss) {
    Score result;
    Score alpha = -VALUE_INFINITE;
    Score beta = VALUE_INFINITE;

    int delta = asp_delta;
    if(depth >= asp_depth) {
        Score avg_score = rootmoves[multipv_idx].avg_score;
        alpha = std::max(avg_score - delta, -int(VALUE_INFINITE));
        beta = std::min(avg_score + delta, int(VALUE_INFINITE));
    }

    int fail_high_count = 0;
    while(true) {
        result = negamax(std::max(1, root_depth - fail_high_count), alpha, beta, ss, false);

        if(is_limit_reached(depth))
            return 0;

        if(id == 0                                                            //
           && debug                                                           //
           && limit.multipv == 1                                              //
           && (result <= alpha || result >= beta) && tm.elapsed_time() > 5000 //
        ) {
            print_uci_info();
        }

        sort_rootmoves(multipv_idx);

        if(result <= alpha) {
            // if result is lower than alpha, than we can lower alpha for next iteration
            beta = (alpha + beta) / 2;
            alpha = std::max(alpha - delta, -int(VALUE_INFINITE));
            fail_high_count = 0;
        } else if(result >= beta) {
            // if result is higher than beta, than we can raise beta for next iteration
            beta = std::min(beta + delta, int(VALUE_INFINITE));
            if(std::abs(result) < VALUE_TB_WIN_IN_MAX_PLY)
                fail_high_count++;
        } else {
            break;
        }

        delta += delta / 3;
    }

    sort_rootmoves(0);

    return result;
}

Score Search::negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node) {
    assert(alpha < beta);
    assert(ss->ply >= 0);

    // variables
    const bool root_node = ss->ply == 0;
    const bool pv_node = beta - alpha != 1;
    const bool in_check = board.in_check();
    bool improving = false;

    const U64 hash = board.get_hash();

    const Score old_alpha = alpha;
    Score eval = ss->static_eval;
    Score raw_eval = eval;
    Score max_score = VALUE_MATE;
    Score best_score = -VALUE_MATE;

    ss->move_count = 0;

    if(pv_node)
        pv_table[ss->ply].length = ss->ply;

    // do quiescence search if depth is less than 1
    if(depth <= 0)
        return qsearch(0, alpha, beta, ss);

    // check for upcoming repetition
    if(!root_node && alpha < VALUE_DRAW && board.has_upcoming_repetition(ss->ply)) {
        alpha = VALUE_DRAW;
        if(alpha >= beta)
            return alpha;
    }

    // check for terminal state
    if(!root_node) {
        if(ss->ply >= MAX_PLY - 1)
            return evaluate();
        if(is_limit_reached(depth))
            return 0;

        // mate distance pruning
        alpha = std::max(alpha, Score(ss->ply - VALUE_MATE));
        beta = std::min(beta, Score(VALUE_MATE - ss->ply - 1));
        if(alpha >= beta)
            return alpha;

        if(board.is_draw(ss->ply))
            return VALUE_DRAW;
    }

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = !ss->skipped ? tt.lookup(hash, &tt_hit) : nullptr;

    Move tt_move = NO_MOVE;
    Bound tt_bound = NO_BOUND;
    Score tt_score = VALUE_NONE;
    Score tt_eval = VALUE_NONE;
    int tt_depth = 0;
    bool tt_pv = pv_node;

    if(tt_hit) {
        tt_move = root_node ? rootmoves[multipv_idx].move : ent->get_move();
        tt_bound = ent->get_bound();
        tt_score = ent->get_score(ss->ply);
        tt_eval = ent->get_eval();
        tt_depth = ent->get_depth();
        tt_pv |= ent->get_tt_pv();
    }

    if(!pv_node                                                       //
       && tt_depth >= depth                                           //
       && tt_score != VALUE_NONE                                      //
       && board.halfmoveclock() < 90                                  // idea from stockfish
       && (tt_bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND)) //
    ) {
        return tt_score;
    }

    // tablebase probing
    if(use_tb && !root_node) {
        Score tb_score = probe_wdl(board);

        if(tb_score != VALUE_NONE) {
            Bound bound = NO_BOUND;
            tb_hits++;

            if(tb_score == VALUE_TB_WIN) {
                tb_score = VALUE_MATE - ss->ply;
                bound = LOWER_BOUND;
            } else if(tb_score == -VALUE_TB_WIN) {
                tb_score = -VALUE_MATE + ss->ply + 1;
                bound = UPPER_BOUND;
            } else {
                tb_score = 0;
                bound = EXACT_BOUND;
            }

            if(bound == EXACT_BOUND                           //
               || (bound == LOWER_BOUND && tb_score >= beta)  //
               || (bound == UPPER_BOUND && tb_score <= alpha) //
            ) {
                ent->store(   //
                    hash,     // hash
                    NO_MOVE,  // move
                    tb_score, // score
                    tt_eval,  // eval
                    bound,    // bound
                    depth,    // depth
                    ss->ply,  // ply
                    tt_pv     // pv
                );
                return tb_score;
            }

            if(pv_node) {
                if(bound == LOWER_BOUND) {
                    best_score = tb_score;
                    alpha = std::max(alpha, best_score);
                } else {
                    max_score = tb_score;
                }
            }
        }
    }

    // set eval and static eval
    if(in_check)
        raw_eval = eval = ss->static_eval = VALUE_NONE;
    else {
        // use tt score for better evaluation
        if(tt_hit) {
            raw_eval = tt_eval == VALUE_NONE ? evaluate() : tt_eval;
            eval = ss->static_eval = adjust_eval(ss, raw_eval);

            if(tt_score != VALUE_NONE                                        //
               && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)) //
            ) {
                eval = tt_score;
            }
        } else if(!ss->skipped) {
            raw_eval = evaluate();
            eval = ss->static_eval = adjust_eval(ss, raw_eval);

            ent->store(     //
                hash,       // hash
                NO_MOVE,    // move
                VALUE_NONE, // score
                raw_eval,   // eval
                NO_BOUND,   // bound
                0,          // depth
                ss->ply,    // ply
                tt_pv       // pv
            );
        }

        // check for improvement
        if((ss - 2)->static_eval != VALUE_NONE)
            improving = ss->static_eval > (ss - 2)->static_eval;
        else if((ss - 4)->static_eval != VALUE_NONE)
            improving = ss->static_eval > (ss - 4)->static_eval;
    }

    // reset killer
    (ss + 1)->killer = NO_MOVE;

    // update quiet history
    Move prev_move = (ss - 1)->curr_move;
    if(!ss->skipped                           //
       && prev_move.is_valid()                // first check if move is valid
       && !is_cap(prev_move)                  //
       && (ss - 1)->static_eval != VALUE_NONE //
    ) {
        int bonus = std::clamp(                                             //
            static_h_mult * (ss->static_eval + (ss - 1)->static_eval) / 16, // value
            -int(static_h_min),                                             // min
            int(static_h_max)                                               // max
        );

        history.update_qh(~board.get_stm(), prev_move, bonus);
    }

    // only use pruning when not in check and pv node
    if(!in_check && !pv_node) {
        // razoring
        if(depth < rzr_depth && eval + rzr_depth_mult * depth < alpha) {
            Score score = qsearch(0, alpha, beta, ss);
            if(score <= alpha)
                return score;
        }

        // reverse futility pruning
        int rfp_margin = rfp_depth_mult * depth - rfp_improving_mult * improving;
        if(!ss->skipped                      //
           && depth < rfp_depth              //
           && eval - rfp_margin >= beta      //
           && eval < VALUE_TB_WIN_IN_MAX_PLY //
        ) {
            return (eval + beta) / 2;
        }

        // null move pruning
        int nmp_margin = nmp_depth_mult * depth - nmp_base;
        if(!ss->skipped                            //
           && depth >= 4                           //
           && eval >= beta                         //
           && board.nonpawnmat()                   //
           && beta > -VALUE_TB_WIN_IN_MAX_PLY      //
           && (ss - 1)->curr_move != NULL_MOVE     //
           && ss->static_eval + nmp_margin >= beta //
        ) {
            int R = nmp_rbase + depth / nmp_rdepth_div + std::min(int(nmp_rmin), (eval - beta) / nmp_eval_div);

            ss->curr_move = NULL_MOVE;
            ss->moved_piece = NO_PIECE;
            ss->conth = &history.conth[0][WHITE_PAWN][a1]; // put null move to quiet moves

            board.make_nullmove();
            Score score = -negamax(depth - R, -beta, -beta + 1, ss + 1, !cut_node);
            board.unmake_nullmove();

            if(score >= beta) {
                // don't return unproven mate scores
                if(score >= VALUE_TB_WIN_IN_MAX_PLY)
                    score = beta;
                return score;
            }
        }

        // internal iterative reduction
        if(!ss->skipped             //
           && !in_check             //
           && !tt_move              //
           && depth >= 4            //
           && (pv_node || cut_node) //
        ) {
            depth--;
        }

        // probcut
        int beta_cut = beta + prob_cut_margin;
        if(!ss->skipped                                       //
           && depth > 4                                       //
           && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY        //
           && !(tt_depth >= depth - 3 && tt_score < beta_cut) //
        ) {
            MovePicker mp(PC_SEARCH, board, history, ss, tt_move);
            mp.see_cutoff = beta_cut > ss->static_eval;

            Move move = NO_MOVE;
            while((move = mp.next()) != NO_MOVE) {
                if(!board.is_legal(move))
                    continue;

                // prefetch tt entry
                tt.prefetch(board.key_after(move));

                nodes++;
                ss->curr_move = move;
                ss->moved_piece = board.piece_at(move.from());
                ss->conth = &history.conth[is_cap(move)][ss->moved_piece][move.to()];

                board.make_move(move);
                Score score = -qsearch(0, -beta_cut, -beta_cut + 1, ss + 1);

                if(score >= beta_cut)
                    score = -negamax(depth - 4, -beta_cut, -beta_cut + 1, ss + 1, !cut_node);

                board.unmake_move(move);

                if(score >= beta_cut) {
                    ent->store(      //
                        hash,        // hash
                        move,        // move
                        score,       // score
                        raw_eval,    // eval
                        LOWER_BOUND, // bound
                        depth - 3,   // depth
                        ss->ply,     // ply
                        tt_pv        // pv
                    );
                    return score;
                }
            }
        }
    }

    // main moves loop
    MovePicker mp(N_SEARCH, board, history, ss, tt_move);

    int made_moves = 0, q_count = 0, c_count = 0;

    Move q_moves[64], c_moves[32];
    Move best_move = NO_MOVE, move;

    while((move = mp.next()) != NO_MOVE) {
        if(move == ss->skipped || !board.is_legal(move))
            continue;
        if(root_node && !found_rootmove(move))
            continue;

        U64 start_nodes = nodes;

        // prefetch tt entry
        tt.prefetch(board.key_after(move));

        made_moves++;
        ss->move_count = made_moves;
        ss->was_cap = is_cap(move);

        int history_score = is_cap(move) ? history.get_ch(board, move) //
                                         : history.get_qh(board, ss, move);

        if(!root_node && best_score > -VALUE_TB_WIN_IN_MAX_PLY) {
            int lmr_depth = std::max(0, depth - REDUCTIONS[depth][made_moves] + history_score / hp_div);

            // late move pruning
            if(!pv_node && q_count > (3 + depth * depth) / (2 - improving))
                mp.skip_quiets();

            if(!is_cap(move) && move.type() != PQ_QUEEN) {
                // history pruning
                if(history_score < -hp_depth_mult * depth && lmr_depth < hp_depth)
                    continue;

                // futility pruning
                if(!in_check                                                   //
                   && lmr_depth < fp_depth                                     //
                   && ss->static_eval + fp_base + lmr_depth * fp_mult <= alpha //
                ) {
                    mp.skip_quiets();
                }
            }

            // see pruning
            int see_margin = is_cap(move) ? -see_cap_margin : -see_quiet_margin;
            if(!board.see(move, (is_cap(move) ? depth : lmr_depth * lmr_depth) * see_margin))
                continue;
        }

        if(is_cap(move) && c_count < 32)
            c_moves[c_count++] = move;
        else if(q_count < 64)
            q_moves[q_count++] = move;

        // print current move information
        if(id == 0 &&               //
           debug &&                 //
           root_node &&             //
           limit.multipv == 1 &&    //
           !threads.is_stopped() && //
           tm.elapsed_time() > 5000 //
        ) {
            std::cout << "info depth " << depth //
                      << " currmove " << move   //
                      << " currmovenumber " << made_moves << std::endl;
        }

        int extensions = 0;

        // singular extensions
        if(!ss->skipped                                    //
           && !root_node                                   //
           && depth >= 6                                   //
           && tt_move == move                              //
           && tt_depth >= depth - 3                        //
           && ss->ply < 2 * root_depth                     //
           && (tt_bound & LOWER_BOUND)                     //
           && std::abs(tt_score) < VALUE_TB_WIN_IN_MAX_PLY //
        ) {
            Score sbeta = tt_score - 6 * depth / 8;
            ss->skipped = move;
            Score score = negamax((depth - 1) / 2, sbeta - 1, sbeta, ss, cut_node);
            ss->skipped = NO_MOVE;

            if(score < sbeta) {
                // if we didn't find a better move, then extend
                if(!pv_node && score < sbeta - 14)
                    extensions = 2 + (!is_cap(move) && !is_prom(move) && score < sbeta - ext_margin);
                else
                    extensions = 1;
            } else if(sbeta >= beta)
                return sbeta;
            else if(tt_score >= beta)
                extensions = -3;
            else if(cut_node)
                extensions = -2;
        }

        int new_depth = depth - 1 + extensions;

        nodes++;

        ss->curr_move = move;
        ss->moved_piece = board.piece_at(move.from());
        ss->conth = &history.conth[is_cap(move)][ss->moved_piece][move.to()];

        board.make_move(move);

        Score score = VALUE_NONE;

        // late move reductions
        if(depth >= 2 && made_moves >= lmr_min_moves && (!tt_pv || !is_cap(move))) {
            int r = REDUCTIONS[depth][made_moves];

            r += !improving;

            r += 2 * cut_node;

            r += (tt_move.is_valid() ? is_cap(tt_move) : 0);

            r -= tt_pv;

            r -= board.in_check();

            r -= (tt_depth >= depth);

            r -= history_score / (is_cap(move) ? hp_cdiv : hp_qdiv);

            int lmr_depth = std::clamp(new_depth - r, 1, new_depth + 1);

            score = -negamax(lmr_depth, -alpha - 1, -alpha, ss + 1, true);

            // if late move reduction failed high and we actually reduced, do a research
            if(score > alpha && lmr_depth < new_depth) {
                // credits to stockfish
                new_depth += (score > best_score + zws_margin);
                new_depth -= (score < best_score + new_depth);

                if(lmr_depth < new_depth)
                    score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1, !cut_node);

                if(!is_cap(move)) {
                    int bonus;
                    if(score <= alpha)
                        bonus = -history_bonus(new_depth);
                    else if(score >= beta)
                        bonus = history_bonus(new_depth);
                    else
                        bonus = 0;

                    history.update_conth(move, ss, bonus);
                }
            }
        } else if(!pv_node || made_moves > 1) {
            // full-depth search if lmr was skipped
            score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1, !cut_node);
        }

        // principal variation search
        if(pv_node && ((score > alpha && (score < beta || root_node)) || made_moves == 1))
            score = -negamax(new_depth, -beta, -alpha, ss + 1, false);

        board.unmake_move(move);

        assert(valid_score(score));

        if(is_limit_reached(depth))
            return 0;

        if(root_node) {
            RootMove *rm = &rootmoves[0];
            for(int i = 1; i < rootmoves.size(); i++) {
                if(rootmoves[i].move == move) {
                    rm = &rootmoves[i];
                    break;
                }
            }

            rm->nodes += nodes - start_nodes;
            rm->avg_score = rm->avg_score == -VALUE_INFINITE ? score : (rm->avg_score + score) / 2;

            if(made_moves == 1 || score > alpha) {
                rm->score = score;

                // copy pv line
                rm->pv[0] = move;

                rm->pv.length = pv_table[ss->ply + 1].length;
                for(int i = 1; i < rm->pv.length; i++)
                    rm->pv[i] = pv_table[ss->ply + 1][i];
            } else {
                rm->score = -VALUE_INFINITE;
            }
        }

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;

                if(pv_node && !root_node)
                    update_pv(move, ss->ply);
            }

            if(alpha >= beta) {
                history.update(                                 //
                    board,                                      //
                    move, ss,                                   //
                    q_moves, q_count,                           //
                    c_moves, c_count,                           //
                    depth + (best_score > beta + hbonus_margin) //
                );
                break; // cut-off
            }
        }
    }

    // check for mate/stalemate
    if(made_moves == 0) {
        if(ss->skipped != NO_MOVE)
            return alpha;
        else if(in_check)
            return -VALUE_MATE + ss->ply;
        else
            return VALUE_DRAW;
    }

    best_score = std::min(best_score, max_score);

    Bound bound;
    if(best_score >= beta)
        bound = LOWER_BOUND;
    else if(best_score <= old_alpha)
        bound = UPPER_BOUND;
    else
        bound = EXACT_BOUND;

    if(!ss->skipped && (!root_node || multipv_idx == 0)) {
        ent->store(     //
            hash,       // hash
            best_move,  // move
            best_score, // score
            raw_eval,   // eval
            bound,      // bound
            depth,      // depth
            ss->ply,    // ply
            tt_pv       // pv
        );
    }

    // update correction histories
    if(!in_check                                                                //
       && (!best_move || !is_cap(best_move))                                    //
       && (bound & (best_score >= ss->static_eval ? LOWER_BOUND : UPPER_BOUND)) //
    ) {
        history.update_contcorr(raw_eval, best_score, depth, ss);
        history.update_matcorr(board, raw_eval, best_score, depth);
    }

    return best_score;
}

Score Search::qsearch(int depth, Score alpha, Score beta, Stack *ss) {
    assert(ss->ply >= 0);
    assert(alpha < beta);

    if(is_limit_reached(1))
        return 0;

    if(alpha < VALUE_DRAW && board.has_upcoming_repetition(ss->ply)) {
        alpha = VALUE_DRAW;
        if(alpha >= beta)
            return alpha;
    }

    if(board.is_draw(ss->ply))
        return VALUE_DRAW;
    if(ss->ply >= MAX_PLY - 1)
        return evaluate();

    // variables
    const bool pv_node = beta - alpha != 1;
    const bool in_check = board.in_check();

    const U64 hash = board.get_hash();

    Score best_score = -VALUE_MATE + ss->ply;
    Score eval = ss->static_eval;
    Score raw_eval = eval;
    Score futility = -VALUE_MATE;

    Move best_move = NO_MOVE;
    Move move;

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);

    Move tt_move = NO_MOVE;
    Bound tt_bound = NO_BOUND;
    Score tt_score = VALUE_NONE;
    Score tt_eval = VALUE_NONE;
    bool tt_pv = pv_node;

    if(tt_hit) {
        tt_move = ent->get_move();
        tt_bound = ent->get_bound();
        tt_score = ent->get_score(ss->ply);
        tt_eval = ent->get_eval();
        tt_pv |= ent->get_tt_pv();
    }

    if(!pv_node                                                       //
       && tt_score != VALUE_NONE                                      //
       && (tt_bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND)) //
    ) {
        return tt_score;
    }

    // set eval and static eval
    if(in_check)
        raw_eval = eval = ss->static_eval = VALUE_NONE;
    else {
        // use tt score for better evaluation
        if(tt_hit) {
            raw_eval = tt_eval == VALUE_NONE ? evaluate() : tt_eval;
            eval = ss->static_eval = adjust_eval(ss, raw_eval);

            if(tt_score != VALUE_NONE                                        //
               && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)) //
            ) {
                eval = tt_score;
            }
        } else {
            raw_eval = evaluate();
            eval = ss->static_eval = adjust_eval(ss, raw_eval);
            ent->store(     //
                hash,       // hash
                NO_MOVE,    // move
                VALUE_NONE, // score
                raw_eval,   // eval
                NO_BOUND,   // bound
                0,          // depth
                ss->ply,    // ply
                tt_pv       // pv
            );
        }

        best_score = eval;
        futility = eval + qfp_margin;

        // stand pat
        if(best_score >= beta) {
            if(std::abs(best_score) < VALUE_TB_WIN_IN_MAX_PLY)
                best_score = (best_score + beta) / 2;
            if(!tt_hit) {
                ent->store(           //
                    hash,             // hash
                    NO_MOVE,          // move
                    VALUE_NONE,       // score
                    raw_eval,         // eval
                    NO_BOUND,         // bound
                    ent->get_depth(), // depth
                    ss->ply,          // ply
                    tt_pv             // pv
                );
            }
            return best_score;
        }

        if(best_score > alpha)
            alpha = best_score;
    }

    (ss + 1)->killer = NO_MOVE;

    MovePicker mp(Q_SEARCH, board, history, ss, tt_move, depth >= -1);

    int made_moves = 0;
    while((move = mp.next()) != NO_MOVE) {
        if(!board.is_legal(move))
            continue;

        // prefetch tt entry
        tt.prefetch(board.key_after(move));

        made_moves++;

        if(best_score > -VALUE_TB_WIN_IN_MAX_PLY) {
            if(!in_check && futility <= alpha && is_cap(move) && !board.see(move, 1)) {
                best_score = std::max(best_score, futility);
                continue;
            }

            // see pruning
            if(!board.see(move, 0))
                continue;
        }

        nodes++;

        ss->curr_move = move;
        ss->moved_piece = board.piece_at(move.from());
        ss->conth = &history.conth[is_cap(move)][ss->moved_piece][move.to()];

        board.make_move(move);
        Score score = -qsearch(depth - 1, -beta, -alpha, ss + 1);
        board.unmake_move(move);

        assert(valid_score(score));

        // update the best score
        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;
            }

            if(alpha >= beta)
                break; // cut-off
        }

        if(best_score > -VALUE_TB_WIN_IN_MAX_PLY)
            mp.skip_quiets();
    }

    if(in_check && made_moves == 0)
        return -VALUE_MATE + ss->ply;

    if(best_score >= beta && std::abs(best_score) < VALUE_TB_WIN_IN_MAX_PLY)
        best_score = (best_score + beta) / 2;

    // store in transposition table
    Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND; // no exact bound in qsearch

    ent->store(     //
        hash,       // hash
        best_move,  // move
        best_score, // score
        raw_eval,   // eval
        bound,      // bound
        0,          // depth
        ss->ply,    // ply
        tt_pv       // pv
    );

    return best_score;
}

// search eval
Score Search::evaluate() {
    int32_t eval = NNUE::nnue.forward(board);
    // scale based on phase
    eval = (128 + board.get_phase()) * eval / 128;
    return std::clamp(eval, -(VALUE_MATE - MAX_PLY) + 1, VALUE_MATE - MAX_PLY - 1);
}

Score Search::adjust_eval(const Stack *ss, Score eval) const {
    eval += (history.get_matcorr(board) + history.get_contcorr(ss)) / 256;

    return std::clamp(                       //
        eval,                                // value
        Score(-VALUE_TB_WIN_IN_MAX_PLY + 1), // min
        Score(VALUE_TB_WIN_IN_MAX_PLY - 1)   // max
    );
}

// search helper

bool Search::is_limit_reached(int depth) const {
    if(threads.is_stopped())
        return true;
    if(limit.infinite)
        return false;
    if(depth > limit.depth)
        return true;
    if(limit.nodes != 0 && nodes >= limit.nodes)
        return true;
    if(limit.time.max != 0 && tm.elapsed_time() >= limit.time.max)
        return true;

    return false;
}

void Search::update_pv(Move move, int ply) {
    pv_table[ply][ply] = move;
    for(int nply = ply + 1; nply < pv_table[ply + 1].length; nply++)
        pv_table[ply][nply] = pv_table[ply + 1][nply];
    pv_table[ply].length = pv_table[ply + 1].length;
}

void Search::sort_rootmoves(int offset) {
    assert(offset >= 0);

    for(int i = offset; i < rootmoves.size(); i++) {
        int best = i;
        for(int j = i + 1; j < rootmoves.size(); j++)
            if(rootmoves[j].score > rootmoves[i].score)
                best = j;

        if(best != i)
            std::swap(rootmoves[i], rootmoves[best]);
    }
}

bool Search::found_rootmove(const Move &move) {
    for(int i = multipv_idx; i < rootmoves.size(); i++)
        if(rootmoves[i].move == move)
            return true;
    return false;
}

void Search::print_uci_info() {
    const int64_t elapsed_time = tm.elapsed_time();
    const U64 total_nodes = Astra::threads.get_nodes();
    const Score result = rootmoves[multipv_idx].score;
    const PVLine &pv_line = rootmoves[multipv_idx].pv;

    std::cout << "info depth " << root_depth    //
              << " multipv " << multipv_idx + 1 //
              << " score ";                     //

    if(std::abs(result) >= VALUE_MATE - MAX_PLY)
        std::cout << "mate " << (VALUE_MATE - std::abs(result) + 1) / 2 * (result > 0 ? 1 : -1);
    else
        std::cout << "cp " << Score(result / 2); // normalize

    std::cout << " nodes " << total_nodes                           //
              << " nps " << total_nodes * 1000 / (elapsed_time + 1) //
              << " tbhits " << Astra::threads.get_tb_hits()         //
              << " hashfull " << Astra::tt.hashfull()               //
              << " time " << elapsed_time                           //
              << " pv " << rootmoves[multipv_idx].move;

    // print rest of the pv
    for(int i = 1; i < pv_line.length; i++)
        std::cout << " " << pv_line[i];

    std::cout << std::endl;
}

} // namespace Astra
