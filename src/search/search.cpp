#include <algorithm>
#include <cmath>

#include "../chess/movegen.h"
#include "../nnue/nnue.h"
#include "../third_party/fathom/src/tbprobe.h"

#include "movepicker.h"
#include "search.h"
#include "threads.h"
#include "tune_params.h"

namespace search {

void Search::idle() {
    threads.add_started_thread();

    while (!exiting) {
        std::unique_lock lock(mutex);
        cv.wait(lock, [&] { return searching; });

        if (exiting)
            return;

        start();
        searching = false;

        cv.notify_all();
    }
}

void Search::clear_histories() {
    quiet_history.clear();
    noisy_history.clear();
    pawn_history.clear();
    cont_history.clear();
    corr_histories.clear();
    cont_corr_history.clear();
}

void Search::start() {
    tm.start();

    nodes = 0;
    tb_hits = 0;
    nmp_min_ply = 0;
    completed_depth = 0;

    if (limits.search_moves.size() > 0) {
        MoveList<Move> legal_moves;
        legal_moves.gen<ADD_LEGALS>(board);

        for (const auto& m : legal_moves) {
            std::ostringstream ss;
            ss << m;
            if (std::find(limits.search_moves.begin(), limits.search_moves.end(), ss.str()) !=
                limits.search_moves.end())
                root_moves.add(RootMove(m));
        }
    } else {
        root_moves.gen<ADD_LEGALS>(board);
    }

    limits.multipv = std::min(limits.multipv, root_moves.size());

    const bool is_main_thread = (this == threads.main_thread());

    accum_list.reset(board);

    if (is_main_thread)
        tt.increment();

    Stack stack_arr[MAX_PLY + 6]; // +6 for continuation history
    Stack* stack = stack_arr + 6;
    for (int i = 0; i < MAX_PLY + 6; i++) {
        stack_arr[i].ply = i - 6;
        stack_arr[i].static_eval = SCORE_NONE;
        stack_arr[i].cont_hist = cont_history.get();
        stack_arr[i].cont_corr_hist = cont_corr_history.get();
    }

    int stability = 0;
    Score scores[MAX_PLY];
    Move best_move = Move::none();
    Move prev_best_move = Move::none();

    for (root_depth = 1; root_depth <= std::min(MAX_PLY, limits.depth); root_depth++) {
        for (multipv_idx = 0; multipv_idx < limits.multipv; multipv_idx++)
            aspiration(root_depth, stack);

        if (threads.is_stopped())
            break;

        completed_depth = root_depth;

        if (is_main_thread && !limits.minimal)
            for (multipv_idx = 0; multipv_idx < limits.multipv; multipv_idx++)
                print_uci_info();

        Score score = root_moves[0].score;
        best_move = root_moves[0];

        stability = (best_move == prev_best_move) ? std::min<int>(stability + 1, tm_stability_max) : 0;

        if (is_main_thread && root_depth >= 5 && limits.time.optimum) {
            double stability_factor = (tm_stability_base / 100.0) - stability * (tm_stability_mult / 1000.0);

            double result_change_factor = (tm_results_base / 100.0)                                        //
                                          + (tm_results_mult1 / 1000.0) * (scores[root_depth - 1] - score) //
                                          + (tm_results_mult2 / 1000.0) * (scores[root_depth - 3] - score);

            result_change_factor = std::clamp(result_change_factor, tm_results_min / 100.0, tm_results_max / 100.0);

            double node_ratio = root_moves[0].nodes / static_cast<double>(nodes);
            double node_count_factor = (1.0 - node_ratio) * (tm_node_mult / 100.0) + (tm_node_base / 100.0);

            // check if we should stop
            if (tm.elapsed_time() > limits.time.optimum * stability_factor * result_change_factor * node_count_factor)
                break;
        }

        prev_best_move = best_move;
        scores[root_depth] = score;
    }

    if (!is_main_thread)
        return;

    threads.stop();
    threads.wait(false);

    multipv_idx = 0;

    Search* best_thread = threads.pick_best();
    if (best_thread != this) {
        best_thread->print_uci_info();
        best_move = best_thread->root_moves[0];
    } else if (limits.minimal) {
        print_uci_info();
    }

    std::cout << "bestmove " << best_move << std::endl;
}

Score Search::aspiration(int depth, Stack* stack) {
    sel_depth = 0;

    Score score;
    Score alpha = -SCORE_INFINITE;
    Score beta = SCORE_INFINITE;

    int delta = asp_delta;
    if (depth >= asp_depth) {
        Score avg_score = root_moves[multipv_idx].avg_score;
        alpha = std::max(avg_score - delta, -SCORE_INFINITE);
        beta = std::min(avg_score + delta, SCORE_INFINITE);
    }

    int fail_high_count = 0;
    while (true) {
        score = negamax<ROOT>(std::max(1, root_depth - fail_high_count), alpha, beta, stack);

        sort_root_moves(multipv_idx);

        if (threads.is_stopped())
            return 0;

        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(alpha - delta, -SCORE_INFINITE);
            fail_high_count = 0;
        } else if (score >= beta) {
            beta = std::min(beta + delta, SCORE_INFINITE);
            fail_high_count++;
        } else {
            break;
        }

        delta += delta / 3;
    }

    sort_root_moves(0);

    return score;
}

template <NodeType nt>
Score Search::negamax(int depth, Score alpha, Score beta, Stack* stack, bool cut_node) {

    constexpr bool root_node = (nt == ROOT);
    constexpr bool pv_node = (nt != NON_PV);

    assert(stack->ply >= 0);
    assert(!(pv_node && cut_node));
    assert(pv_node || (alpha == beta - 1));
    assert(-SCORE_INFINITE <= alpha && alpha < beta && beta <= SCORE_INFINITE);

    if (pv_node)
        stack->pv.length = stack->ply;

    if (is_limit_reached()) {
        threads.stop();
        return 0;
    }

    if (threads.is_stopped())
        return 0;

    if (depth <= 0)
        return quiescence<nt>(alpha, beta, stack);

    depth = std::min(depth, MAX_PLY - 1);

    // check for upcoming repetition
    if (!root_node && alpha < SCORE_DRAW && board.upcoming_repetition(stack->ply)) {
        alpha = SCORE_DRAW;
        if (alpha >= beta)
            return alpha;
    }

    const Score old_alpha = alpha;
    const Color stm = board.side_to_move();
    const bool in_check = board.in_check();
    const U64 hash = board.hash();

    if (pv_node)
        sel_depth = std::max(sel_depth, stack->ply);

    if (!root_node) {
        if (stack->ply >= MAX_PLY - 1)
            return in_check ? SCORE_DRAW : evaluate();
        if (board.is_draw(stack->ply))
            return SCORE_DRAW;

        // mate distance pruning
        alpha = std::max(mated_in(stack->ply), alpha);
        beta = std::min(mate_in(stack->ply + 1), beta);
        if (alpha >= beta)
            return alpha;
    }

    Score best_score = -SCORE_INFINITE;
    Score max_score = SCORE_INFINITE;
    Score raw_eval, eval, probcut_beta;
    Move best_move = Move::none();
    bool improving = false;

    // look up in tt
    bool tt_hit = false;
    auto* ent = tt.lookup(hash, &tt_hit);

    Move tt_move = tt_hit ? ent->get_move() : Move::none();
    Bound tt_bound = tt_hit ? ent->get_bound() : NO_BOUND;
    Score tt_score = tt_hit ? ent->get_score(stack->ply) : SCORE_NONE;
    Score tt_eval = tt_hit ? ent->get_eval() : SCORE_NONE;
    int tt_depth = tt_hit ? ent->get_depth() : 0;
    bool tt_pv = pv_node || (tt_hit && ent->get_tt_pv());

    if (root_node && valid_score(root_moves[multipv_idx].score))
        tt_move = root_moves[multipv_idx];

    if (!pv_node                                         //
        && !stack->skipped                               //
        && valid_score(tt_score)                         //
        && tt_depth > depth - (tt_score <= beta)         //
        && valid_tt_score(tt_score, beta, tt_bound)      //
        && (cut_node == (tt_score >= beta) || depth > 5) //
    ) {
        if (tt_move && tt_score >= beta) {
            if (tt_move.is_quiet()) {
                int bonus = std::min<int>(tt_hist_bonus_mult * depth + tt_hist_bonus_minus, max_tt_hist_bonus);
                quiet_history.update(stm, tt_move, bonus);
            }

            Square prev_sq = (stack - 1)->move ? (stack - 1)->move.to() : NO_SQUARE;
            if (valid_sq(prev_sq) && !(stack - 1)->move.is_cap() && (stack - 1)->move_count <= 3) {
                int malus = std::min<int>(tt_hist_malus_mult * depth + tt_hist_malus_minus, max_tt_hist_malus);
                cont_history.update((stack - 1)->moved_piece, prev_sq, -malus, stack - 1);
            }
        }

        if (board.fifty_move_count() < 90)
            return tt_score;
    }

    // tablebase probing
    if (!root_node && !stack->skipped) {
        auto tb_result = probe_wdl();

        if (tb_result != TB_RESULT_FAILED) {
            Bound tb_bound;
            Score tb_score;
            tb_hits.store(tb_hits.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);

            if (tb_result == TB_LOSS) {
                tb_score = stack->ply - SCORE_TB;
                tb_bound = UPPER_BOUND;
            } else if (tb_result == TB_WIN) {
                tb_score = SCORE_TB - stack->ply;
                tb_bound = LOWER_BOUND;
            } else {
                tb_score = SCORE_DRAW;
                tb_bound = EXACT_BOUND;
            }

            if (tb_bound == EXACT_BOUND                           //
                || (tb_bound == LOWER_BOUND && tb_score >= beta)  //
                || (tb_bound == UPPER_BOUND && tb_score <= alpha) //
            ) {
                ent->store(hash, Move::none(), tb_score, SCORE_NONE, tb_bound, depth, stack->ply, tt_pv);
                return tb_score;
            }

            if (pv_node) {
                if (tb_bound == LOWER_BOUND) {
                    best_score = tb_score;
                    alpha = std::max(alpha, best_score);
                } else {
                    max_score = tb_score;
                }
            }
        }
    }

    // set eval and static eval
    if (in_check) {
        raw_eval = eval = stack->static_eval = SCORE_NONE;
        goto movesloop;
    } else if (stack->skipped)
        raw_eval = eval = stack->static_eval;
    else {
        raw_eval = valid_score(tt_eval) ? tt_eval : evaluate();
        eval = stack->static_eval = adjust_eval(raw_eval, stack);

        if (valid_score(tt_score) && valid_tt_score(tt_score, eval + 1, tt_bound))
            eval = tt_score;
        else if (!tt_hit)
            ent->store(hash, Move::none(), SCORE_NONE, raw_eval, NO_BOUND, 0, stack->ply, tt_pv);
    }

    if (valid_score((stack - 2)->static_eval))
        improving = stack->static_eval > (stack - 2)->static_eval;
    else if (valid_score((stack - 4)->static_eval))
        improving = stack->static_eval > (stack - 4)->static_eval;

    // update quiet history
    if (!stack->skipped                          //
        && (stack - 1)->move                     //
        && (stack - 1)->move.is_quiet()          //
        && valid_score((stack - 1)->static_eval) //
    ) {
        int bonus = std::clamp<int>(
            static_h_mult * (stack->static_eval + (stack - 1)->static_eval) / 16, static_h_min, static_h_max
        );

        quiet_history.update(~stm, (stack - 1)->move, bonus);
    }

    // razoring
    if (!pv_node && eval < alpha - rzr_base - rzr_mult * depth * depth)
        return quiescence<NON_PV>(alpha, beta, stack);

    // reverse futility pruning
    if (!pv_node                                                                    //
        && !is_win(eval)                                                            //
        && !is_loss(beta)                                                           //
        && !stack->skipped                                                          //
        && depth < rfp_depth                                                        //
        && eval - (rfp_depth_mult * depth - rfp_improving_mult * improving) >= beta //
    ) {
        return (eval + beta) / 2;
    }

    // null move pruning
    if (cut_node                                                          //
        && eval >= beta                                                   //
        && !is_loss(beta)                                                 //
        && !stack->skipped                                                //
        && board.non_pawn_mat(stm)                                        //
        && stack->ply >= nmp_min_ply                                      //
        && stack->static_eval + nmp_depth_mult * depth - nmp_base >= beta //
    ) {
        assert(!(stack - 1)->move.is_null());

        int R = nmp_rbase                //
                + depth / nmp_rdepth_div //
                + std::min<int>(nmp_rmin, (eval - beta) / nmp_eval_div);

        stack->move = Move::null();
        stack->moved_piece = NO_PIECE;
        stack->cont_hist = cont_history.get();
        stack->cont_corr_hist = cont_corr_history.get();

        board.make_move();
        Score score = -negamax<NON_PV>(depth - R, -beta, -beta + 1, stack + 1, !cut_node);
        board.undo_move();

        if (threads.is_stopped())
            return 0;

        if (score >= beta && !is_win(score)) {
            if (nmp_min_ply || depth < 16)
                return score;

            assert(!nmp_min_ply);

            nmp_min_ply = stack->ply + 3 * (depth - R) / 4;
            Score v = negamax<NON_PV>(depth - R, beta - 1, beta, stack, false);
            nmp_min_ply = 0;

            if (v >= beta)
                return score;
        }
    }

    // internal iterative reduction
    if (depth >= iir_depth && !tt_move && (pv_node || cut_node))
        depth--;

    // probcut
    probcut_beta = beta + probcut_margin - 60 * improving;
    if (!pv_node                                               //
        && depth >= 3                                          //
        && !is_decisive(beta)                                  //
        && !(valid_score(tt_score) && tt_score < probcut_beta) //
    ) {
        MovePicker<PC_SEARCH> mp(board, tt_move, quiet_history, pawn_history, noisy_history, stack);
        mp.probcut_threshold = probcut_beta - stack->static_eval;

        const int probcut_depth = std::max(depth - 4, 0);

        Move move = Move::none();
        while ((move = mp.next())) {
            if (move == stack->skipped || !board.legal(move))
                continue;

            make_move(move, stack);
            Score score = -quiescence<NON_PV>(-probcut_beta, -probcut_beta + 1, stack + 1);

            if (score >= probcut_beta && probcut_depth > 0)
                score = -negamax<NON_PV>(probcut_depth, -probcut_beta, -probcut_beta + 1, stack + 1, !cut_node);

            undo_move(move);

            if (threads.is_stopped())
                return 0;

            if (score >= probcut_beta) {
                ent->store(hash, move, score, raw_eval, LOWER_BOUND, probcut_depth + 1, stack->ply, tt_pv);

                if (!is_decisive(score))
                    return score - (probcut_beta - beta);
            }
        }
    }

movesloop:

    MovePicker<N_SEARCH> mp(board, tt_move, quiet_history, pawn_history, noisy_history, stack);
    Move move = Move::none();

    MoveList<Move> quiets, noisy;
    int move_count = 0;

    bool skip_quiets = false;
    while ((move = mp.next(skip_quiets))) {
        if (move == stack->skipped || !board.legal(move))
            continue;
        if (root_node && !found_root_move(move))
            continue;

        U64 start_nodes = nodes;

        stack->move_count = ++move_count;

        int r = reduction(depth, move_count);

        int history_score = 0;
        if (move.is_quiet()) {
            Piece pc = board.piece_at(move.from());

            history_score = quiet_history.get(board.side_to_move(), move)                //
                            + pawn_history.get(board, move)                              //
                            + static_cast<int>((*(stack - 1)->cont_hist)[pc][move.to()]) //
                            + static_cast<int>((*(stack - 2)->cont_hist)[pc][move.to()]);
        } else {
            history_score = noisy_history.get(board, move);
        }

        if (!root_node && !is_loss(best_score)) {
            // late move pruning
            if (quiets.size() > (3 + depth * depth) / (2 - improving))
                skip_quiets = true;

            if (move.is_quiet()) {
                const int r_depth = std::max(0, depth - r + history_score / hist_div);

                // futility pruning
                const Score futility = stack->static_eval + fp_base + r_depth * fp_mult;
                if (!in_check && r_depth < fp_depth && futility <= alpha)
                    skip_quiets = true;

                // history pruning
                if (r_depth < hp_depth && history_score < hp_depth_mult * depth) {
                    skip_quiets = true;
                    continue;
                }

                // see pruning
                if (!board.see(move, r_depth * r_depth * see_quiet_margin))
                    continue;
            } else {
                // see pruning
                if (!board.see(move, depth * see_noisy_margin))
                    continue;
            }
        }

        // singular extensions
        int extensions = 0;

        if (!root_node                     //
            && depth >= 6                  //
            && !stack->skipped             //
            && move == tt_move             //
            && tt_depth >= depth - 3       //
            && valid_score(tt_score)       //
            && !is_decisive(tt_score)      //
            && tt_bound & LOWER_BOUND      //
            && stack->ply < 2 * root_depth //
        ) {
            Score sbeta = tt_score - (1 + tt_pv && !pv_node) * depth;

            stack->skipped = move;
            Score score = negamax<NON_PV>((depth - 1) / 2, sbeta - 1, sbeta, stack, cut_node);
            stack->skipped = Move::none();

            if (threads.is_stopped())
                return 0;

            if (score < sbeta) {
                extensions = 1;
                if (!pv_node && score < sbeta - double_ext_margin)
                    extensions = 2 + (move.is_quiet() && score < sbeta - tripple_ext_margin);
            } else if (sbeta >= beta && !is_decisive(sbeta)) {
                return sbeta;
            } else if (tt_score >= beta) {
                extensions = -3;
            } else if (cut_node) {
                extensions = -2;
            }
        }

        int new_depth = depth - 1 + extensions;

        make_move(move, stack);

        Score score = SCORE_NONE;

        // late move reductions
        if (depth >= 2 && move_count >= 3 && !(tt_pv && move.is_noisy())) {

            r += !improving;

            r += 2 * cut_node;

            r += (tt_move && tt_move.is_noisy());

            r -= tt_pv;

            r -= board.in_check();

            r -= (tt_depth >= depth);

            r -= history_score / (move.is_quiet() ? quiet_hist_div : noisy_hist_div);

            const int r_depth = std::clamp(new_depth - r, 1, new_depth + 1);

            score = -negamax<NON_PV>(r_depth, -alpha - 1, -alpha, stack + 1, true);

            if (score > alpha && new_depth > r_depth) {
                new_depth += (score > best_score + zws_margin + 2 * new_depth);
                new_depth -= (score < best_score + new_depth);

                if (new_depth > r_depth)
                    score = -negamax<NON_PV>(new_depth, -alpha - 1, -alpha, stack + 1, !cut_node);

                if (!move.is_cap()) {
                    int bonus = 0;
                    if (score <= alpha)
                        bonus = -history_bonus(new_depth);
                    else if (score >= beta)
                        bonus = history_bonus(new_depth);

                    cont_history.update(stack->moved_piece, move.to(), bonus, stack);
                }
            }
        }
        // full depth search if lmr was skipped
        else if (!pv_node || move_count > 1) {
            score = -negamax<NON_PV>(new_depth, -alpha - 1, -alpha, stack + 1, !cut_node);
        }

        // principal variation search
        if (pv_node && (move_count == 1 || score > alpha))
            score = -negamax<PV>(new_depth, -beta, -alpha, stack + 1);

        undo_move(move);

        assert(valid_score(score));

        if (threads.is_stopped())
            return 0;

        if (root_node) {
            const int move_idx = root_moves.idx_of(RootMove(move));
            assert(move_idx >= 0);
            RootMove* rm = &root_moves[move_idx];

            rm->nodes += nodes - start_nodes;
            rm->avg_score = valid_score(rm->avg_score) ? (rm->avg_score + score) / 2 : score;

            if (move_count == 1 || score > alpha) {
                rm->score = score;
                rm->sel_depth = sel_depth;
                rm->pv = (stack + 1)->pv;
                rm->pv[0] = move;
            } else {
                rm->score = -SCORE_INFINITE;
            }
        }

        if (score > best_score) {
            best_score = score;

            if (score > alpha) {
                best_move = move;

                if (pv_node && !root_node)
                    update_pv(move, stack);

                if (score >= beta) {
                    update_histories(best_move, quiets, noisy, depth + (best_score > beta + hist_bonus_margin), stack);
                    break;
                }

                if (depth > 2 && depth < 14 && !is_decisive(score))
                    depth--;

                alpha = score;
            }
        }

        if (move != best_move && move_count <= 32) {
            if (move.is_quiet())
                quiets.add(move);
            else
                noisy.add(move);
        }
    }

    // check for mate/stalemate
    if (!move_count) {
        if (stack->skipped)
            return alpha;
        return in_check ? mated_in(stack->ply) : SCORE_DRAW;
    }

    if (pv_node)
        best_score = std::min(best_score, max_score);

    // store in tt
    Bound bound = EXACT_BOUND;
    if (best_score >= beta)
        bound = LOWER_BOUND;
    else if (best_score <= old_alpha)
        bound = UPPER_BOUND;

    if (!stack->skipped && !(root_node && multipv_idx))
        ent->store(hash, best_move, best_score, raw_eval, bound, depth, stack->ply, tt_pv);

    // update correction histories
    if (!in_check                                                //
        && !(best_move && best_move.is_cap())                    //
        && valid_tt_score(best_score, stack->static_eval, bound) //
    ) {
        int bonus = std::clamp((best_score - raw_eval) * depth / 8, -256, 256);
        corr_histories.update(board, bonus);
        cont_corr_history.update(board, bonus, stack);
    }

    assert(valid_score(best_score));

    return best_score;
}

template <NodeType nt>
Score Search::quiescence(Score alpha, Score beta, Stack* stack) {

    constexpr bool pv_node = (nt != NON_PV);

    assert(stack->ply >= 0);
    assert(pv_node || (alpha == beta - 1));
    assert(-SCORE_INFINITE <= alpha && alpha < beta && beta <= SCORE_INFINITE);

    if (pv_node)
        stack->pv.length = stack->ply;

    if (pv_node)
        sel_depth = std::max(sel_depth, stack->ply);

    if (threads.is_stopped())
        return 0;

    if (board.is_draw(stack->ply))
        return SCORE_DRAW;

    // check for upcoming repetition
    if (alpha < SCORE_DRAW && board.upcoming_repetition(stack->ply)) {
        alpha = SCORE_DRAW;
        if (alpha >= beta)
            return alpha;
    }

    const bool in_check = board.in_check();
    const U64 hash = board.hash();

    if (stack->ply >= MAX_PLY - 1)
        return in_check ? SCORE_DRAW : evaluate();

    Move best_move = Move::none();

    Score best_score = -SCORE_INFINITE;
    Score raw_eval, futility;

    // look up in tt
    bool tt_hit = false;
    auto* ent = tt.lookup(hash, &tt_hit);

    Move tt_move = tt_hit ? ent->get_move() : Move::none();
    Bound tt_bound = tt_hit ? ent->get_bound() : NO_BOUND;
    Score tt_score = tt_hit ? ent->get_score(stack->ply) : SCORE_NONE;
    Score tt_eval = tt_hit ? ent->get_eval() : SCORE_NONE;
    bool tt_pv = tt_hit && ent->get_tt_pv();

    if (!pv_node && valid_score(tt_score) && valid_tt_score(tt_score, beta, tt_bound))
        return tt_score;

    if (in_check) {
        futility = raw_eval = stack->static_eval = SCORE_NONE;
    } else {
        raw_eval = valid_score(tt_eval) ? tt_eval : evaluate();
        best_score = stack->static_eval = adjust_eval(raw_eval, stack);
        futility = best_score + qfp_margin;

        if (valid_score(tt_score) && valid_tt_score(tt_score, best_score + 1, tt_bound))
            best_score = tt_score;

        // stand pat
        if (best_score >= beta) {
            if (!is_decisive(best_score))
                best_score = (best_score + beta) / 2;
            if (!tt_hit)
                ent->store(hash, Move::none(), SCORE_NONE, raw_eval, NO_BOUND, 0, stack->ply, false);
            return best_score;
        }

        if (best_score > alpha)
            alpha = best_score;
    }

    const Square prev_sq = ((stack - 1)->move) ? ((stack - 1)->move).to() : NO_SQUARE;

    MovePicker<Q_SEARCH> mp(board, tt_move, quiet_history, pawn_history, noisy_history, stack);
    Move move = Move::none();

    int move_count = 0;
    while ((move = mp.next())) {
        if (!board.legal(move))
            continue;

        move_count++;

        if (!is_loss(best_score)) {
            // if we are not losing while in check, we can skip quiet moves
            if (in_check && move.is_quiet())
                break;

            if (prev_sq != move.to() && !move.is_prom() && !board.gives_check(move)) {
                if (move_count > 2)
                    break;

                // futility pruning
                if (!in_check && futility <= alpha && !board.see(move, 1)) {
                    best_score = std::max(best_score, futility);
                    continue;
                }
            }

            // see pruning
            if (!board.see(move, qsee_margin))
                continue;
        }

        make_move(move, stack);
        Score score = -quiescence<nt>(-beta, -alpha, stack + 1);
        undo_move(move);

        assert(valid_score(score));

        if (threads.is_stopped())
            return 0;

        if (score > best_score) {
            best_score = score;

            if (score > alpha) {
                alpha = score;
                best_move = move;

                if (pv_node)
                    update_pv(move, stack);
                if (alpha >= beta)
                    break;
            }
        }
    }

    if (in_check && !move_count)
        return mated_in(stack->ply);
    if (best_score >= beta && !is_decisive(best_score))
        best_score = (best_score + beta) / 2;

    Bound bound = (best_score >= beta) ? LOWER_BOUND : UPPER_BOUND;
    ent->store(hash, best_move, best_score, raw_eval, bound, 0, stack->ply, tt_pv);

    assert(valid_score(best_score));

    return best_score;
}

void Search::make_move(Move move, Stack* stack) {
    assert(stack != nullptr);

    nodes.store(nodes.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);

    stack->move = move;
    stack->moved_piece = board.piece_at(move.from());

    auto dirty_pieces = board.make_move(move);

    accum_list.increment(dirty_pieces);

    stack->cont_hist = cont_history.get(board.in_check(), move.is_cap(), stack->moved_piece, move.to());
    stack->cont_corr_hist = cont_corr_history.get(stack->moved_piece, move.to());

    tt.prefetch(board.hash());
}

void Search::undo_move(Move move) {
    accum_list.decrement();
    board.undo_move(move);
}

int Search::reduction(int depth, int move_count) const {
    double base = lmr_base / 100.0;
    double div_factor = lmr_div / 100.0;
    return base + log(depth) * log(move_count) / div_factor;
}

Score Search::evaluate() {
    assert(accum_list[0].initialized[WHITE]);
    assert(accum_list[0].initialized[BLACK]);

    auto& acc = accum_list.back();

    for (Color c : {WHITE, BLACK}) {
        if (acc.initialized[c])
            continue;

        const int accums_idx = accum_list.size() - 1;
        assert(accums_idx > 0);

        // apply lazy update
        for (int i = accums_idx; i >= 0; i--) {
            if (accum_list[i].needs_refresh(c)) {
                accum_list.refresh(c, board);
                break;
            }

            if (accum_list[i].initialized[c]) {
                for (int j = i + 1; j <= accums_idx; j++)
                    accum_list[j].update(accum_list[j - 1], c);
                break;
            }
        }
    }

    int eval = nnue::nnue.forward(board, accum_list.back());

    int material = 122 * board.count<PAWN>()     //
                   + 401 * board.count<KNIGHT>() //
                   + 427 * board.count<BISHOP>() //
                   + 663 * board.count<ROOK>()   //
                   + 1237 * board.count<QUEEN>();

    eval = (230 + material / 42) * eval / 440;

    return std::clamp(eval, -SCORE_MATE_IN_MAX_PLY, SCORE_MATE_IN_MAX_PLY);
}

Score Search::adjust_eval(int32_t eval, Stack* stack) const {
    eval = (eval * (200 - board.fifty_move_count())) / 200;
    eval += (corr_histories.get(board) + cont_corr_history.get(board, stack)) / 256;

    return std::clamp(eval, SCORE_TB_LOSS_IN_MAX_PLY + 1, SCORE_TB_WIN_IN_MAX_PLY - 1);
}

unsigned int Search::probe_wdl() const {
    U64 w_occ = board.occupancy(WHITE);
    U64 b_occ = board.occupancy(BLACK);
    U64 occ = w_occ | b_occ;

    if (pop_count(occ) > signed(TB_LARGEST))
        return TB_RESULT_FAILED;

    Square ep_sq = board.en_passant();

    return tb_probe_wdl(
        w_occ,
        b_occ,
        board.piece_bb<KING>(WHITE),
        board.piece_bb<QUEEN>(WHITE),
        board.piece_bb<ROOK>(WHITE),
        board.piece_bb<BISHOP>(WHITE),
        board.piece_bb<KNIGHT>(WHITE),
        board.piece_bb<PAWN>(WHITE),
        board.fifty_move_count(),
        board.state().castling_rights.any(),
        valid_sq(ep_sq) ? ep_sq : 0,
        board.side_to_move() == WHITE
    );
}

bool Search::is_limit_reached() const {
    if (this != threads.main_thread())
        return false; // only main thread checks limits
    if (limits.infinite)
        return false;
    if (limits.nodes && threads.total_nodes() >= limits.nodes)
        return true;
    if (limits.time.maximum && tm.elapsed_time() >= limits.time.maximum)
        return true;
    return false;
}

void Search::sort_root_moves(int offset) {
    assert(offset >= 0);
    auto it = root_moves.begin() + offset;
    std::stable_sort(it, root_moves.end(), [](const RootMove& a, const RootMove& b) { return a.score > b.score; });
}

bool Search::found_root_move(Move move) {
    for (int i = multipv_idx; i < root_moves.size(); i++)
        if (root_moves[i] == move)
            return true;
    return false;
}

void Search::update_pv(Move move, Stack* stack) {
    stack->pv.length = (stack + 1)->pv.length;
    stack->pv[stack->ply] = move;

    for (int i = stack->ply + 1; i < stack->pv.length; i++)
        stack->pv[i] = (stack + 1)->pv[i];
}

void Search::update_quiet_histories(Move best_move, int bonus, Stack* stack) {
    quiet_history.update(board.side_to_move(), best_move, bonus);
    pawn_history.update(board, best_move, bonus);
    cont_history.update(board.piece_at(best_move.from()), best_move.to(), bonus, stack);
}

void Search::update_histories(Move best_move, MoveList<Move>& quiets, MoveList<Move>& noisy, int depth, Stack* stack) {
    int bonus = history_bonus(depth);
    int malus = -history_malus(depth);

    if (best_move.is_noisy()) {
        noisy_history.update(board, best_move, bonus * noisy_hist_bonus_mult / 1024);
    } else if (depth > 3 || quiets.size() > 0) {
        update_quiet_histories(best_move, bonus * quiet_hist_bonus_mult / 1024, stack);

        int quiet_malus = malus * quiet_hist_malus_mult / 1024;
        for (const auto& m : quiets)
            update_quiet_histories(m, quiet_malus, stack);
    }

    for (const auto& m : noisy)
        noisy_history.update(board, m, malus * noisy_hist_malus_mult / 1024);
}

void Search::print_uci_info() const {
    const auto& rm = root_moves[multipv_idx];
    const int64_t elapsed_time = tm.elapsed_time();
    const U64 total_nodes = threads.total_nodes();

    std::cout << "info depth " << completed_depth //
              << " seldepth " << rm.sel_depth     //
              << " multipv " << multipv_idx + 1   //
              << " score ";                       //

    if (std::abs(rm.score) >= SCORE_MATE_IN_MAX_PLY)
        std::cout << "mate " << (SCORE_MATE - std::abs(rm.score) + 1) / 2 * (rm.score > 0 ? 1 : -1);
    else
        std::cout << "cp " << (100 * rm.score / 160);

    std::cout << " nodes " << total_nodes                           //
              << " nps " << total_nodes * 1000 / (elapsed_time + 1) //
              << " tbhits " << threads.tb_hits()                    //
              << " hashfull " << tt.hashfull()                      //
              << " time " << elapsed_time                           //
              << " pv " << rm;

    for (int i = 1; i < rm.pv.length; i++) {
        Move move = rm.pv[i];
        if (!move)
            break;
        std::cout << " " << move;
    }

    std::cout << std::endl;
}

} // namespace search
