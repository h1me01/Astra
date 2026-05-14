#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>

#include "../chess/movegen.h"
#include "../nnue/nnue.h"
#include "../third_party/fathom/src/tbprobe.h"
#include "../util.h"
#include "movepicker.h"
#include "search.h"
#include "threads.h"
#include "tune_params.h"
#include "types.h"

namespace astra::search {

void Search::idle() {
    thread_pool.add_started_thread();

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
    quiet_history_.clear();
    noisy_history_.clear();
    pawn_history_.clear();
    cont_history_.clear();
    corr_histories_.clear();
    cont_corr_history_.clear();
}

void Search::start() {
    tm_.start();

    nodes_ = 0;
    tb_hits_ = 0;
    nmp_min_ply_ = 0;
    completed_depth_ = 0;
    root_moves_.clear();

    { // initialize root moves
        MoveList<Move> ml;
        gen_moves<GenType::LEGAL>(ml, board);

        for (Move m : ml) {
            auto s = std::format("{}", m);
            if (limits.search_moves.empty() ||
                std::ranges::any_of(limits.search_moves, [&](const auto& sm) { return sm == s; })) {
                root_moves_.add(RootMove(m));
            }
        }
    }

    limits.multipv = std::min(limits.multipv, root_moves_.size());
    accum_stack_.reset(board);

    const bool is_main_thread = (this == thread_pool.main_thread());

    if (is_main_thread)
        tt.increment();

    NDArray<Stack, MAX_PLY + 6> stack_arr; // +6 for continuation history
    Stack* stack = stack_arr.data() + 6;
    for (size_t i = 0; i < stack_arr.total; ++i) {
        stack_arr(i).ply = i - 6;
        stack_arr(i).static_eval = SCORE_NONE;
        stack_arr(i).cont_hist = cont_history_.get();
        stack_arr(i).cont_corr_hist = cont_corr_history_.get();
    }

    int stability = 0;
    NDArray<Score, MAX_PLY> scores;
    Move prev_best_move = Move::none();

    for (root_depth_ = 1; root_depth_ <= std::min(MAX_PLY, limits.depth); ++root_depth_) {
        for (multipv_idx_ = 0; multipv_idx_ < limits.multipv; ++multipv_idx_)
            aspiration(root_depth_, stack);

        if (thread_pool.is_stopped())
            break;

        completed_depth_ = root_depth_;

        if (is_main_thread && !limits.minimal)
            for (multipv_idx_ = 0; multipv_idx_ < limits.multipv; ++multipv_idx_)
                print_uci_info();

        Move best_move = root_moves_[0];
        Score score = root_moves_[0].score;

        stability = (best_move == prev_best_move) ? std::min<int>(stability + 1, tm_stability_max) : 0;

        if (is_main_thread && root_depth_ >= 5 && limits.time.optimum) {
            double stability_factor = (tm_stability_base / 100.0) - stability * (tm_stability_mult / 1000.0);

            double result_change_factor = (tm_results_base / 100.0)                                         //
                                          + (tm_results_mult1 / 1000.0) * (scores(root_depth_ - 1) - score) //
                                          + (tm_results_mult2 / 1000.0) * (scores(root_depth_ - 3) - score);

            result_change_factor = std::clamp(result_change_factor, tm_results_min / 100.0, tm_results_max / 100.0);

            double node_ratio = root_moves_[0].nodes / static_cast<double>(nodes_);
            double node_count_factor = (1.0 - node_ratio) * (tm_node_mult / 100.0) + (tm_node_base / 100.0);

            // check if we should stop
            if (tm_.elapsed_time() > limits.time.optimum * stability_factor * result_change_factor * node_count_factor)
                break;
        }

        prev_best_move = best_move;
        scores(root_depth_) = score;
    }

    if (!is_main_thread)
        return;

    thread_pool.stop();
    thread_pool.wait(false);

    multipv_idx_ = 0;

    Move best_move = root_moves_[0];
    Search* best_thread = thread_pool.pick_best();
    if (best_thread != this) {
        best_thread->multipv_idx_ = 0;
        best_thread->print_uci_info();
        best_move = best_thread->root_moves_[0];
    } else if (limits.minimal || best_move != prev_best_move) {
        print_uci_info();
    }

    println("bestmove {}", best_move);
}

Score Search::aspiration(int depth, Stack* stack) {
    sel_depth_ = 0;

    Score score;
    Score alpha = -SCORE_INFINITE;
    Score beta = SCORE_INFINITE;

    int delta = asp_delta;
    if (depth >= 4) {
        Score avg_score = root_moves_[multipv_idx_].avg_score;
        alpha = std::max(avg_score - delta, -SCORE_INFINITE);
        beta = std::min(avg_score + delta, SCORE_INFINITE);
    }

    int fail_high_count = 0;
    while (true) {
        score = negamax<ROOT>(std::max(1, root_depth_ - fail_high_count), alpha, beta, stack);
        sort_root_moves(multipv_idx_);

        if (thread_pool.is_stopped())
            return 0;

        if (score <= alpha) {
            beta = (alpha + beta) / 2;
            alpha = std::max(score - delta, -SCORE_INFINITE);
            fail_high_count = 0;
        } else if (score >= beta) {
            beta = std::min(score + delta, SCORE_INFINITE);
            ++fail_high_count;
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
    assert(-SCORE_INFINITE <= alpha);
    assert(alpha < beta);
    assert(beta <= SCORE_INFINITE);

    if (pv_node)
        stack->pv.length = stack->ply;

    if (is_limit_reached()) {
        thread_pool.stop();
        return 0;
    }

    if (depth <= 0)
        return quiescence<nt>(alpha, beta, stack);

    depth = std::min(depth, MAX_PLY - 1);

    // check for upcoming repetition
    if (!root_node && alpha < SCORE_DRAW && board.upcoming_repetition(stack->ply)) {
        alpha = draw_score();
        if (alpha >= beta)
            return alpha;
    }

    const Score old_alpha = alpha;
    const Color stm = board.side_to_move();
    const bool in_check = board.in_check();
    const Hash hash = board.hash();

    if (pv_node)
        sel_depth_ = std::max(sel_depth_, stack->ply);

    if (!root_node) {
        if (stack->ply >= MAX_PLY - 1)
            return in_check ? draw_score() : evaluate();
        if (board.is_draw(stack->ply))
            return draw_score();

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

    Move tt_move = tt_hit ? ent->move() : Move::none();
    Bound tt_bound = tt_hit ? ent->bound() : NO_BOUND;
    Score tt_score = tt_hit ? ent->score(stack->ply) : SCORE_NONE;
    Score tt_eval = tt_hit ? ent->eval() : SCORE_NONE;
    int tt_depth = tt_hit ? ent->depth() : 0;
    bool tt_pv = pv_node || (tt_hit && ent->tt_pv());

    if (root_node && is_valid(root_moves_[multipv_idx_].score))
        tt_move = root_moves_[multipv_idx_];

    if (!pv_node                                         //
        && !stack->skipped                               //
        && is_valid(tt_score)                            //
        && tt_depth > depth - (tt_score <= beta)         //
        && valid_tt_score(tt_score, beta, tt_bound)      //
        && (cut_node == (tt_score >= beta) || depth > 5) //
    ) {
        if (tt_move && tt_score >= beta) {
            if (tt_move.is_quiet()) {
                int bonus = std::min<int>(tt_hist_bonus_mult * depth + tt_hist_bonus_bias, tt_hist_bonus_max);
                quiet_history_.update(stm, tt_move, bonus);
            }

            Square prev_sq = (stack - 1)->move ? (stack - 1)->move.to() : NO_SQUARE;
            if (is_valid(prev_sq) && (stack - 1)->move.is_quiet() && (stack - 1)->move_count <= 3) {
                int malus = std::min<int>(tt_hist_malus_mult * depth + tt_hist_malus_bias, tt_hist_malus_max);
                cont_history_.update((stack - 1)->moved_piece, prev_sq, -malus, stack - 1);
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
            tb_hits_.fetch_add(1, std::memory_order_relaxed);

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
        raw_eval = is_valid(tt_eval) ? tt_eval : evaluate();
        eval = stack->static_eval = adjust_eval(raw_eval, stack);

        if (is_valid(tt_score) && valid_tt_score(tt_score, eval + 1, tt_bound))
            eval = tt_score;
        else if (!tt_hit)
            ent->store(hash, Move::none(), SCORE_NONE, raw_eval, NO_BOUND, 0, stack->ply, tt_pv);
    }

    if (is_valid((stack - 2)->static_eval))
        improving = stack->static_eval > (stack - 2)->static_eval;
    else if (is_valid((stack - 4)->static_eval))
        improving = stack->static_eval > (stack - 4)->static_eval;

    // update quiet history
    if (!stack->skipped                       //
        && (stack - 1)->move                  //
        && (stack - 1)->move.is_quiet()       //
        && is_valid((stack - 1)->static_eval) //
    ) {
        int bonus = std::clamp<int>(
            static_hist_mult * (stack->static_eval + (stack - 1)->static_eval) / 16, static_hist_min, static_hist_max
        );

        quiet_history_.update(~stm, (stack - 1)->move, bonus);
    }

    // razoring
    if (!pv_node && eval < alpha - rzr_base - rzr_mult * depth * depth)
        return quiescence<NON_PV>(alpha, beta, stack);

    // reverse futility pruning
    if (!pv_node                                                                    //
        && depth < 11                                                               //
        && !is_win(eval)                                                            //
        && !is_loss(beta)                                                           //
        && !stack->skipped                                                          //
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
        && stack->ply >= nmp_min_ply_                                     //
        && stack->static_eval + nmp_depth_mult * depth - nmp_base >= beta //
    ) {
        assert(!(stack - 1)->move.is_null());

        int R = 4           //
                + depth / 3 //
                + std::min<int>(4, (eval - beta) / nmp_eval_div);

        stack->move = Move::null();
        stack->moved_piece = NO_PIECE;
        stack->cont_hist = cont_history_.get();
        stack->cont_corr_hist = cont_corr_history_.get();

        board.make_move();
        Score score = -negamax<NON_PV>(depth - R, -beta, -beta + 1, stack + 1, !cut_node);
        board.undo_move();

        if (thread_pool.is_stopped())
            return 0;

        if (score >= beta && !is_win(score)) {
            if (nmp_min_ply_ || depth < 16)
                return score;

            assert(!nmp_min_ply_);

            nmp_min_ply_ = stack->ply + 3 * (depth - R) / 4;
            Score v = negamax<NON_PV>(depth - R, beta - 1, beta, stack, false);
            nmp_min_ply_ = 0;

            if (v >= beta)
                return score;
        }
    }

    // internal iterative reduction
    if (depth >= 4 && !tt_move && (pv_node || cut_node))
        depth--;

    // probcut
    probcut_beta = beta + pc_margin - pc_improving_mult * improving;
    if (!pv_node                                            //
        && depth >= pc_depth                                //
        && !is_decisive(beta)                               //
        && !(is_valid(tt_score) && tt_score < probcut_beta) //
    ) {
        MovePicker<PC_SEARCH> mp(
            board, tt_move, quiet_history_, pawn_history_, noisy_history_, stack, probcut_beta - stack->static_eval
        );

        const int probcut_depth = std::max(depth - 4, 0);

        Move move = Move::none();
        while ((move = mp.next())) {
            if (move == stack->skipped || !board.is_legal(move))
                continue;

            make_move(move, stack);
            Score score = -quiescence<NON_PV>(-probcut_beta, -probcut_beta + 1, stack + 1);

            if (score >= probcut_beta && probcut_depth > 0)
                score = -negamax<NON_PV>(probcut_depth, -probcut_beta, -probcut_beta + 1, stack + 1, !cut_node);

            undo_move(move);

            if (thread_pool.is_stopped())
                return 0;

            if (score >= probcut_beta) {
                ent->store(hash, move, score, raw_eval, LOWER_BOUND, probcut_depth + 1, stack->ply, tt_pv);

                if (!is_decisive(score))
                    return score - (probcut_beta - beta);
            }
        }
    }

movesloop:

    MovePicker<N_SEARCH> mp(board, tt_move, quiet_history_, pawn_history_, noisy_history_, stack);
    Move move = Move::none();

    MoveList<Move> quiets, noisy;
    int move_count = 0;

    while ((move = mp.next())) {
        if (move == stack->skipped || !board.is_legal(move))
            continue;
        if (root_node && !found_root_move(move))
            continue;

        uint64_t start_nodes = nodes_;

        stack->move_count = ++move_count;

        int r = reduction(depth, move_count);

        int history_score = 0;
        if (move.is_quiet()) {
            auto pc = board.piece_at(move.from());

            history_score = quiet_history_.get(board.side_to_move(), move) //
                            + pawn_history_.get(board, move)               //
                            + (*(stack - 1)->cont_hist)(pc, move.to())     //
                            + (*(stack - 2)->cont_hist)(pc, move.to());
        } else {
            history_score = noisy_history_.get(board, move);
        }

        if (!root_node && !is_loss(best_score)) {
            // late move pruning
            if (quiets.size() > (3 + depth * depth) / (2 - improving))
                mp.skip_quiets();

            if (move.is_quiet()) {
                const int r_depth = std::max(0, depth - r / 1024 + history_score / hist_div);

                // futility pruning
                const Score futility = stack->static_eval + fp_base + r_depth * fp_mult;
                if (!in_check && r_depth < 10 && futility <= alpha)
                    mp.skip_quiets();

                // history pruning
                if (r_depth < 6 && history_score < hp_depth_mult * depth) {
                    mp.skip_quiets();
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

        if (!root_node                      //
            && depth >= 6                   //
            && !stack->skipped              //
            && move == tt_move              //
            && tt_depth >= depth - 3        //
            && is_valid(tt_score)           //
            && !is_decisive(tt_score)       //
            && (tt_bound & LOWER_BOUND)     //
            && stack->ply < 2 * root_depth_ //
        ) {
            Score sbeta = tt_score - (1 + (tt_pv && !pv_node)) * depth;

            stack->skipped = move;
            Score score = negamax<NON_PV>((depth - 1) / 2, sbeta - 1, sbeta, stack, cut_node);
            stack->skipped = Move::none();

            if (thread_pool.is_stopped())
                return 0;

            if (score < sbeta) {
                extensions = 1;
                if (!pv_node && score < sbeta - double_ext_margin)
                    extensions = 2 + (move.is_quiet() && score < sbeta - triple_ext_margin);
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
        if (depth >= 2 && move_count >= 2) {

            r += !improving * lmr_improving;

            if (cut_node)
                r += lmr_cut_node + !tt_move * lmr_cut_node_no_tt_move;

            r += (tt_move && tt_move.is_noisy()) * lmr_tt_move_noisy;

            if (tt_pv) {
                r -= lmr_tt_pv                            //
                     + (tt_depth >= depth) * lmr_tt_depth //
                     + (is_valid(tt_score) && tt_score > alpha) * lmr_tt_score;
            }

            r -= board.in_check() * lmr_in_check;

            r -= (move.is_quiet() ? lmr_quiet_hist_mul : lmr_noisy_hist_mul) * history_score / 1024;

            const int r_depth = std::clamp(new_depth - r / 1024, 1, new_depth + 1) + pv_node;

            score = -negamax<NON_PV>(r_depth, -alpha - 1, -alpha, stack + 1, true);

            if (score > alpha && new_depth > r_depth) {
                new_depth += (score > best_score + dp_margin);
                new_depth -= (score < best_score + ds_margin);

                if (new_depth > r_depth)
                    score = -negamax<NON_PV>(new_depth, -alpha - 1, -alpha, stack + 1, !cut_node);

                if (move.is_quiet()) {
                    int bonus = 0;
                    if (score <= alpha)
                        bonus = -history_bonus(new_depth);
                    else if (score >= beta)
                        bonus = history_bonus(new_depth);

                    cont_history_.update(stack->moved_piece, move.to(), bonus, stack);
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

        assert(is_valid(score));

        if (thread_pool.is_stopped())
            return 0;

        if (root_node) {
            const int move_idx = root_moves_.idx_of(RootMove(move));
            assert(move_idx >= 0);
            RootMove* rm = &root_moves_[move_idx];

            rm->nodes += nodes_ - start_nodes;
            rm->avg_score = is_valid(rm->avg_score) ? (rm->avg_score + score) / 2 : score;

            if (move_count == 1 || score > alpha) {
                rm->score = score;
                rm->sel_depth = sel_depth_;
                rm->pv = (stack + 1)->pv;
                rm->pv(0) = move;
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

                if (depth > sdr_min_depth && depth < sdr_max_depth && !is_decisive(score))
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

    if (!stack->skipped && !(root_node && multipv_idx_))
        ent->store(hash, best_move, best_score, raw_eval, bound, depth, stack->ply, tt_pv);

    // update correction histories
    if (!in_check                                                //
        && !(best_move && best_move.is_noisy())                  //
        && valid_tt_score(best_score, stack->static_eval, bound) //
    ) {
        int bonus = std::clamp((best_score - stack->static_eval) * depth / 8, -256, 256);
        corr_histories_.update(board, bonus);
        cont_corr_history_.update(board, bonus, stack);
    }

    assert(is_valid(best_score));

    return best_score;
}

template <NodeType nt>
Score Search::quiescence(Score alpha, Score beta, Stack* stack) {

    constexpr bool pv_node = (nt != NON_PV);

    assert(stack->ply >= 0);
    assert(pv_node || (alpha == beta - 1));
    assert(-SCORE_INFINITE <= alpha);
    assert(alpha < beta);
    assert(beta <= SCORE_INFINITE);

    if (pv_node) {
        stack->pv.length = stack->ply;
        sel_depth_ = std::max(sel_depth_, stack->ply);
    }

    if (thread_pool.is_stopped())
        return 0;

    if (board.is_draw(stack->ply))
        return draw_score();

    // check for upcoming repetition
    if (alpha < SCORE_DRAW && board.upcoming_repetition(stack->ply)) {
        alpha = draw_score();
        if (alpha >= beta)
            return alpha;
    }

    const bool in_check = board.in_check();
    const Hash hash = board.hash();

    if (stack->ply >= MAX_PLY - 1)
        return in_check ? draw_score() : evaluate();

    Move best_move = Move::none();

    Score best_score = -SCORE_INFINITE;
    Score raw_eval, futility;

    // look up in tt
    bool tt_hit = false;
    auto* ent = tt.lookup(hash, &tt_hit);

    Move tt_move = tt_hit ? ent->move() : Move::none();
    Bound tt_bound = tt_hit ? ent->bound() : NO_BOUND;
    Score tt_score = tt_hit ? ent->score(stack->ply) : SCORE_NONE;
    Score tt_eval = tt_hit ? ent->eval() : SCORE_NONE;
    bool tt_pv = tt_hit && ent->tt_pv();

    if (!pv_node && is_valid(tt_score) && valid_tt_score(tt_score, beta, tt_bound))
        return tt_score;

    if (in_check) {
        futility = raw_eval = stack->static_eval = SCORE_NONE;
    } else {
        raw_eval = is_valid(tt_eval) ? tt_eval : evaluate();
        best_score = stack->static_eval = adjust_eval(raw_eval, stack);
        futility = best_score + qfp_margin;

        if (is_valid(tt_score) && valid_tt_score(tt_score, best_score + 1, tt_bound))
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

    MovePicker<Q_SEARCH> mp(board, tt_move, quiet_history_, pawn_history_, noisy_history_, stack);
    Move move = Move::none();

    int move_count = 0;
    while ((move = mp.next())) {
        if (!board.is_legal(move))
            continue;

        ++move_count;

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

        assert(is_valid(score));

        if (thread_pool.is_stopped())
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

    assert(is_valid(best_score));

    return best_score;
}

void Search::make_move(Move move, Stack* stack) {
    assert(stack != nullptr);

    nodes_.fetch_add(1, std::memory_order_relaxed);

    stack->move = move;
    stack->moved_piece = board.piece_at(move.from());

    auto dirty_pieces = board.make_move(move);
    accum_stack_.push(dirty_pieces, board.king_sq(WHITE), board.king_sq(BLACK));

    stack->cont_hist = cont_history_.get(board.in_check(), move.is_noisy(), stack->moved_piece, move.to());
    stack->cont_corr_hist = cont_corr_history_.get(stack->moved_piece, move.to());

    tt.prefetch(board.hash());
}

void Search::undo_move(Move move) {
    accum_stack_.pop();
    board.undo_move(move);
}

int Search::reduction(int depth, int move_count) const {
    return lmr_base + lmr_mul * static_cast<int>(std::log(depth) * std::log(move_count));
}

Score Search::evaluate() {
    assert(accum_stack_[0].initialized(WHITE));
    assert(accum_stack_[0].initialized(BLACK));

    auto& acc = accum_stack_.back();

    for (Color c : {WHITE, BLACK}) {
        if (acc.initialized(c))
            continue;

        const int accums_idx = accum_stack_.size() - 1;
        assert(accums_idx > 0);

        // apply lazy update
        for (int i = accums_idx; i >= 0; i--) {
            if (accum_stack_[i].initialized(c)) {
                for (int j = i + 1; j <= accums_idx; ++j)
                    accum_stack_[j].update(accum_stack_[j - 1], c);
                break;
            }

            if (accum_stack_[i].should_refresh(c)) {
                accum_stack_.refresh(c, board);
                break;
            }
        }
    }

    int32_t eval = nnue::nnue.forward(board, accum_stack_.back());

    return std::clamp(eval, -SCORE_MATE_IN_MAX_PLY, SCORE_MATE_IN_MAX_PLY);
}

Score Search::adjust_eval(int32_t eval, Stack* stack) const {
    int material = ms_pawn * board.count<PAWN>()       //
                   + ms_knight * board.count<KNIGHT>() //
                   + ms_bishop * board.count<BISHOP>() //
                   + ms_rook * board.count<ROOK>()     //
                   + ms_queen * board.count<QUEEN>();

    eval = eval * (ms_base + material) / ms_div;

    eval = (eval * (200 - board.fifty_move_count())) / 200;
    eval += (corr_histories_.get(board) + cont_corr_history_.get(board, stack)) / 1024;

    return std::clamp(eval, SCORE_TB_LOSS_IN_MAX_PLY + 1, SCORE_TB_WIN_IN_MAX_PLY - 1);
}

Score Search::draw_score() const { return Score(nodes() & 0x2) - 1; }

unsigned int Search::probe_wdl() const {
    Bitboard w_occ = board.occupancy(WHITE);
    Bitboard b_occ = board.occupancy(BLACK);
    Bitboard occ = w_occ | b_occ;

    if (pop_count(occ) > signed(TB_LARGEST))
        return TB_RESULT_FAILED;

    Square ep_sq = board.en_passant();

    return tb_probe_wdl(
        w_occ,
        b_occ,
        board.piece_bb<KING>(),
        board.piece_bb<QUEEN>(),
        board.piece_bb<ROOK>(),
        board.piece_bb<BISHOP>(),
        board.piece_bb<KNIGHT>(),
        board.piece_bb<PAWN>(),
        board.fifty_move_count(),
        board.state().castling_rights.any(),
        is_valid(ep_sq) ? ep_sq : 0,
        board.side_to_move() == WHITE
    );
}

bool Search::is_limit_reached() const {
    if (this != thread_pool.main_thread())
        return false; // only main thread checks limits
    if (limits.nodes && thread_pool.total_nodes() >= limits.nodes)
        return true;
    if (limits.time.maximum && tm_.elapsed_time() >= limits.time.maximum)
        return true;
    return false;
}

void Search::sort_root_moves(int offset) {
    assert(offset >= 0);
    std::stable_sort(root_moves_.begin() + offset, root_moves_.end(), [](const RootMove& a, const RootMove& b) {
        return a.score > b.score;
    });
}

bool Search::found_root_move(Move move) {
    return std::any_of(root_moves_.begin(), root_moves_.end(), [move](const RootMove& rm) { return rm == move; });
}

void Search::update_pv(Move move, Stack* stack) {
    stack->pv.length = (stack + 1)->pv.length;
    stack->pv(stack->ply) = move;

    for (int i = stack->ply + 1; i < stack->pv.length; ++i)
        stack->pv(i) = (stack + 1)->pv(i);
}

void Search::update_quiet_histories(Move best_move, int bonus, Stack* stack) {
    quiet_history_.update(board.side_to_move(), best_move, bonus);
    pawn_history_.update(board, best_move, bonus);
    cont_history_.update(board.piece_at(best_move.from()), best_move.to(), bonus, stack);
}

void Search::update_histories(Move best_move, MoveList<Move>& quiets, MoveList<Move>& noisy, int depth, Stack* stack) {
    int bonus = history_bonus(depth);
    int malus = -history_malus(depth);

    if (best_move.is_noisy()) {
        noisy_history_.update(board, best_move, noisy_hist_bonus_mult * bonus / 1024);
    } else if (depth > 3 || !quiets.empty()) {
        update_quiet_histories(best_move, quiet_hist_bonus_mult * bonus / 1024, stack);

        int quiet_malus = quiet_hist_malus_mult * malus / 1024;
        for (const auto& m : quiets)
            update_quiet_histories(m, quiet_malus, stack);
    }

    for (const auto& m : noisy)
        noisy_history_.update(board, m, noisy_hist_malus_mult * malus / 1024);
}

void Search::print_uci_info() const {
    const auto& rm = root_moves_[multipv_idx_];
    const int64_t elapsed_time = tm_.elapsed_time();
    const uint64_t total_nodes = thread_pool.total_nodes();

    print("info depth {} seldepth {} multipv {} score ", completed_depth_, rm.sel_depth, multipv_idx_ + 1);

    if (std::abs(rm.score) >= SCORE_MATE_IN_MAX_PLY)
        print("mate {}", (SCORE_MATE - std::abs(rm.score) + 1) / 2 * (rm.score > 0 ? 1 : -1));
    else
        print("cp {}", (rm.score * 100) / 200);

    print(
        " nodes {} nps {} tbhits {} hashfull {} time {} pv {}",
        total_nodes,
        total_nodes * 1000 / (elapsed_time + 1),
        thread_pool.tb_hits(),
        tt.hashfull(),
        elapsed_time,
        static_cast<const Move&>(rm)
    );

    for (int i = 1; i < rm.pv.length; ++i) {
        Move move = rm.pv(i);
        if (!move)
            break;
        print(" {}", move);
    }

    println("");
}

} // namespace astra::search
