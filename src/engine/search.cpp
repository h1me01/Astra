#include <algorithm>
#include <cmath>

#include "../chess/movegen.h"
#include "../nnue/nnue.h"
#include "../third_party/fathom/src/tbprobe.h"

#include "movepicker.h"
#include "search.h"
#include "threads.h"
#include "tune_params.h"

namespace Engine {

int REDUCTIONS[MAX_PLY][MAX_MOVES];

void init_reductions() {
    const double base = double(lmr_base) / 100.0;
    const double div_factor = double(lmr_div) / 100.0;

    for(int depth = 1; depth < MAX_PLY; depth++)
        for(int moves = 1; moves < MAX_MOVES; moves++)
            REDUCTIONS[depth][moves] = base + log(depth) * log(moves) / div_factor;
}

// Search

void Search::start(const Board &board, Limits limits) {
    tm.start();

    this->board = board;
    this->limits = limits;

    total_nodes = 0;
    tb_hits = 0;
    nmp_min_ply = 0;

    root_moves.gen<LEGALS>(board);

    if(!root_moves.size()) {
        std::cout << "Position has no legal moves" << std::endl;
        return;
    }

    const int multipv_size = std::min(limits.multipv, root_moves.size());

    if(!id)
        tt.increment();

    Move best_move = NO_MOVE;
    Move prev_best_move = NO_MOVE;

    // init stack
    Stack stack_array[MAX_PLY + 6]; // +6 for continuation history
    Stack *stack = &stack_array[6];
    for(int i = 0; i < MAX_PLY + 6; i++) {
        stack_array[i].ply = i - 6;
        stack_array[i].conth = &history.conth[0][NO_PIECE][NO_SQUARE];
    }

    int stability = 0;
    Score scores[MAX_PLY];

    for(root_depth = 1; root_depth < MAX_PLY; root_depth++) {
        for(multipv_idx = 0; multipv_idx < multipv_size; multipv_idx++)
            aspiration(root_depth, stack);

        if(is_limit_reached(root_depth))
            break;

        // print final pv info
        for(multipv_idx = 0; multipv_idx < multipv_size; multipv_idx++)
            print_uci_info();

        Score score = root_moves[0].get_score();
        best_move = root_moves[0];

        if(best_move == prev_best_move)
            stability = std::min(stability + 1, int(tm_stability_max));
        else
            stability = 0;

        if(!id                                               //
           && root_depth >= 5                                //
           && tm.should_stop(                                //
                  limits,                                    //
                  stability,                                 //
                  scores[root_depth - 1] - score,            //
                  scores[root_depth - 3] - score,            //
                  root_moves[0].nodes / double(total_nodes)) //
        ) {
            break;
        }

        prev_best_move = best_move;
        scores[root_depth] = score;
    }

    if(!id)
        std::cout << "bestmove " << best_move << std::endl;

    threads.stop();
}

Score Search::aspiration(int depth, Stack *stack) {
    Score score;
    Score alpha = -VALUE_INFINITE;
    Score beta = VALUE_INFINITE;

    int delta = asp_delta;
    if(depth >= asp_depth) {
        Score avg_score = root_moves[multipv_idx].avg_score;
        alpha = std::max(avg_score - delta, int(-VALUE_INFINITE));
        beta = std::min(avg_score + delta, int(VALUE_INFINITE));
    }

    int fail_high_count = 0;
    while(true) {
        score = negamax<NodeType::ROOT>(std::max(1, root_depth - fail_high_count), alpha, beta, stack);

        if(is_limit_reached(depth))
            return 0;

        sort_rootmoves(multipv_idx);

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

    sort_rootmoves(0);

    return score;
}

template <NodeType nt> //
Score Search::negamax(int depth, Score alpha, Score beta, Stack *stack, bool cut_node) {

    constexpr bool root_node = (nt == NodeType::ROOT);
    constexpr bool pv_node = (nt != NodeType::NON_PV);

    assert(stack->ply >= 0);
    assert(!(pv_node && cut_node));
    assert(pv_node || (alpha == beta - 1));
    assert(valid_score(alpha + 1) && valid_score(beta - 1) && alpha < beta);

    if(pv_node)
        stack->pv.length = stack->ply;

    if(is_limit_reached(depth))
        return 0;

    if(depth <= 0)
        return quiescence<nt>(0, alpha, beta, stack);

    depth = std::min(depth, MAX_PLY - 1);

    // check for upcoming repetition
    if(!root_node && alpha < VALUE_DRAW && board.upcoming_repetition(stack->ply)) {
        alpha = VALUE_DRAW;
        if(alpha >= beta)
            return alpha;
    }

    const Score old_alpha = alpha;
    const Color stm = board.get_stm();
    const bool in_check = board.in_check();
    const U64 hash = board.get_hash();

    Score best_score = -VALUE_INFINITE;
    Score max_score = VALUE_INFINITE;
    Score raw_eval, eval, beta_cut;

    Move best_move = NO_MOVE;

    bool improving = false;

    (stack + 1)->killer = NO_MOVE;
    (stack + 1)->skipped = NO_MOVE;

    if(!root_node) {
        if(stack->ply >= MAX_PLY - 1)
            return in_check ? VALUE_DRAW : evaluate();

        if(board.is_draw(stack->ply))
            return VALUE_DRAW;

        // mate distance pruning
        alpha = std::max(alpha, Score(stack->ply - VALUE_MATE));
        beta = std::min(beta, Score(VALUE_MATE - stack->ply - 1));
        if(alpha >= beta)
            return alpha;
    }

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = tt.lookup(hash, &tt_hit);

    Move tt_move = root_node ? root_moves[multipv_idx] : tt_hit ? ent->get_move() : NO_MOVE;
    Bound tt_bound = NO_BOUND;
    Score tt_score = VALUE_NONE;
    Score tt_eval = VALUE_NONE;
    int tt_depth = 0;
    bool tt_pv = pv_node;

    if(tt_hit) {
        tt_bound = ent->get_bound();
        tt_score = ent->get_score(stack->ply);
        tt_eval = ent->get_eval();
        tt_depth = ent->get_depth();
        tt_pv |= ent->get_tt_pv();
    }

    const bool tt_move_noisy = tt_move && !tt_move.is_quiet();

    if(!pv_node                                         //
       && !stack->skipped                               //
       && board.get_fmr() < 90                          //
       && valid_score(tt_score)                         //
       && tt_depth > depth - (tt_score <= beta)         //
       && valid_tt_score(tt_score, beta, tt_bound)      //
       && (cut_node == (tt_score >= beta) || depth > 5) //
    ) {
        return tt_score;
    }

    // tablebase probing
    if(use_tb && !root_node && !stack->skipped) {
        auto tb_result = probe_wdl();

        if(tb_result != TB_RESULT_FAILED) {
            Bound tb_bound;
            Score tb_score;
            tb_hits++;

            if(tb_result == TB_LOSS) {
                tb_score = stack->ply - VALUE_TB;
                tb_bound = UPPER_BOUND;
            } else if(tb_result == TB_WIN) {
                tb_score = VALUE_TB - stack->ply;
                tb_bound = LOWER_BOUND;
            } else {
                tb_score = VALUE_DRAW;
                tb_bound = EXACT_BOUND;
            }

            if(tb_bound == EXACT_BOUND                           //
               || (tb_bound == LOWER_BOUND && tb_score >= beta)  //
               || (tb_bound == UPPER_BOUND && tb_score <= alpha) //
            ) {
                ent->store(     //
                    hash,       //
                    NO_MOVE,    //
                    tb_score,   //
                    VALUE_NONE, // eval
                    tb_bound,   //
                    depth,      //
                    stack->ply, //
                    tt_pv       //
                );

                return tb_score;
            }

            if(pv_node) {
                if(tb_bound == LOWER_BOUND) {
                    best_score = tb_score;
                    alpha = std::max(alpha, best_score);
                } else {
                    max_score = tb_score;
                }
            }
        }
    }

    // set eval and static eval
    if(in_check) {
        raw_eval = eval = stack->static_eval = VALUE_NONE;
        goto movesloop;
    } else if(stack->skipped)
        raw_eval = eval = stack->static_eval;
    else {
        raw_eval = valid_score(tt_eval) ? tt_eval : evaluate();
        eval = stack->static_eval = adjust_eval(raw_eval, stack);

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
                stack->ply, //
                tt_pv       //
            );
        }
    }

    if(valid_score((stack - 2)->static_eval))
        improving = stack->static_eval > (stack - 2)->static_eval;
    else if(valid_score((stack - 4)->static_eval))
        improving = stack->static_eval > (stack - 4)->static_eval;

    // update quiet history
    if(!stack->skipped                          //
       && (stack - 1)->move                     //
       && !(stack - 1)->move.is_cap()           //
       && valid_score((stack - 1)->static_eval) //
    ) {
        int bonus = std::clamp(                                                   //
            static_h_mult * (stack->static_eval + (stack - 1)->static_eval) / 16, //
            int(static_h_min),                                                    //
            int(static_h_max)                                                     //
        );

        history.update_hh(~stm, (stack - 1)->move, bonus);
    }

    // razoring
    if(!pv_node                                 //
       && !is_win(alpha)                        //
       && depth < rzr_depth                     //
       && eval + rzr_depth_mult * depth < alpha //
    ) {
        Score score = quiescence<NodeType::NON_PV>(0, alpha, beta, stack);
        if(score <= alpha)
            return score;
    }

    // reverse futility pruning
    if(!pv_node                                                                    //
       && !stack->skipped                                                          //
       && !is_win(eval)                                                            //
       && !is_loss(beta)                                                           //
       && depth < rfp_depth                                                        //
       && eval - (rfp_depth_mult * depth - rfp_improving_mult * improving) >= beta //
    ) {
        return (eval + beta) / 2;
    }

    // null move pruning
    if(cut_node                                                          //
       && eval >= beta                                                   //
       && !stack->skipped                                                //
       && !is_loss(beta)                                                 //
       && board.nonpawn_mat(stm)                                         //
       && stack->ply >= nmp_min_ply                                      //
       && (stack - 1)->move != NULL_MOVE                                 //
       && stack->static_eval + nmp_depth_mult * depth - nmp_base >= beta //
    ) {
        int R = nmp_rbase                //
                + depth / nmp_rdepth_div //
                + std::min(int(nmp_rmin), (eval - beta) / nmp_eval_div);

        stack->move = NULL_MOVE;
        stack->moved_piece = NO_PIECE;
        stack->conth = &history.conth[0][NO_PIECE][NO_SQUARE];

        board.make_nullmove();
        Score score = -negamax<NodeType::NON_PV>(depth - R, -beta, -beta + 1, stack + 1, !cut_node);
        board.undo_nullmove();

        if(score >= beta && !is_win(score)) {
            if(nmp_min_ply || depth < 16)
                return score;

            assert(!nmp_min_ply);

            nmp_min_ply = stack->ply + 3 * (depth - R) / 4;
            Score v = negamax<NodeType::NON_PV>(depth - R, beta - 1, beta, stack, false);
            nmp_min_ply = 0;

            if(v >= beta)
                return score;
        }
    }

    // internal iterative reduction
    if(depth >= 4 && !tt_move && (pv_node || cut_node))
        depth--;

    // probcut
    beta_cut = beta + prob_cut_margin;
    if(!pv_node                                           //
       && depth > 4                                       //
       && !is_decisive(beta)                              //
       && !(tt_depth >= depth - 3 && tt_score < beta_cut) //
    ) {
        MovePicker mp(PC_SEARCH, board, history, stack, tt_move);
        mp.pb_threshold = beta_cut - stack->static_eval;

        Move move = NO_MOVE;
        while((move = mp.next()) != NO_MOVE) {
            if(!board.is_legal(move))
                continue;

            tt.prefetch(board.key_after(move));

            total_nodes++;
            stack->move = move;
            stack->moved_piece = board.piece_at(move.from());
            stack->conth = &history.conth[move.is_cap()][stack->moved_piece][move.to()];

            board.make_move(move);
            Score score = -quiescence<NodeType::NON_PV>(0, -beta_cut, -beta_cut + 1, stack + 1);

            if(score >= beta_cut)
                score = -negamax<NodeType::NON_PV>(depth - 4, -beta_cut, -beta_cut + 1, stack + 1, !cut_node);

            board.undo_move(move);

            if(score >= beta_cut) {
                ent->store(      //
                    hash,        //
                    move,        //
                    score,       //
                    raw_eval,    //
                    LOWER_BOUND, //
                    depth - 3,   //
                    stack->ply,  //
                    tt_pv        //
                );

                return score;
            }
        }
    }

movesloop:

    MovePicker mp(N_SEARCH, board, history, stack, tt_move);
    Move move = NO_MOVE;

    MoveList<> quiets, noisy;
    int made_moves = 0;

    while((move = mp.next()) != NO_MOVE) {
        if(move == stack->skipped || !board.is_legal(move))
            continue;
        if(root_node && !found_rootmove(move))
            continue;

        tt.prefetch(board.key_after(move));

        U64 start_nodes = total_nodes;

        made_moves++;

        const int reduction = REDUCTIONS[depth][made_moves];
        const int history_score = move.is_quiet() ? history.get_qh(board, move, stack) : history.get_nh(board, move);

        if(!root_node && !is_loss(best_score)) {
            // late move pruning
            if(quiets.size() > (3 + depth * depth) / (2 - improving))
                mp.skip_quiets();

            if(move.is_cap()) {
                // see pruning
                if(!board.see(move, depth * see_cap_margin))
                    continue;
            } else {
                const int r_depth = std::max(0, depth - reduction + history_score / history_div);

                // futility pruning
                const Score futility = stack->static_eval + fp_base + r_depth * fp_mult;
                if(!in_check && r_depth < fp_depth && futility <= alpha)
                    mp.skip_quiets();

                // history pruning
                if(r_depth < hp_depth && history_score < hp_depth_mult * depth) {
                    mp.skip_quiets();
                    continue;
                }

                // see pruning
                if(!board.see(move, r_depth * r_depth * see_quiet_margin))
                    continue;
            }
        }

        // singular extensions
        int extensions = 0;

        if(!root_node                     //
           && depth >= 6                  //
           && !stack->skipped             //
           && move == tt_move             //
           && tt_depth >= depth - 3       //
           && valid_score(tt_score)       //
           && !is_decisive(tt_score)      //
           && tt_bound & LOWER_BOUND      //
           && stack->ply < 2 * root_depth //
        ) {
            Score sbeta = tt_score - 6 * depth / 8;
            stack->skipped = move;
            Score score = negamax<NodeType::NON_PV>((depth - 1) / 2, sbeta - 1, sbeta, stack, cut_node);
            stack->skipped = NO_MOVE;

            if(score < sbeta) {
                extensions = 1;
                if(!pv_node && score < sbeta - double_ext_margin)
                    extensions = 2 + (move.is_quiet() && score < sbeta - tripple_ext_margin);
            } else if(sbeta >= beta)
                return sbeta;
            else if(tt_score >= beta)
                extensions = -3;
            else if(cut_node)
                extensions = -2;
        }

        stack->move = move;
        stack->moved_piece = board.piece_at(move.from());
        stack->conth = &history.conth[move.is_cap()][stack->moved_piece][move.to()];

        total_nodes++;

        int new_depth = depth - 1 + extensions;

        board.make_move(move);

        Score score = VALUE_NONE;

        // late move reductions
        if(depth >= 2 && made_moves >= lmr_min_moves && (!tt_pv || move.is_quiet())) {
            int r = reduction;

            r += !improving;

            r += 2 * cut_node;

            r += tt_move_noisy;

            r -= tt_pv;

            r -= board.in_check();

            r -= (tt_depth >= depth);

            r -= history_score / (move.is_quiet() ? quiet_history_div : noisy_history_div);

            const int r_depth = std::clamp(new_depth - r, 1, new_depth + 1);

            score = -negamax<NodeType::NON_PV>(r_depth, -alpha - 1, -alpha, stack + 1, true);

            if(score > alpha && new_depth > r_depth) {
                new_depth += (score > best_score + zws_margin + 2 * new_depth);
                new_depth -= (score < best_score + new_depth);

                if(new_depth > r_depth)
                    score = -negamax<NodeType::NON_PV>(new_depth, -alpha - 1, -alpha, stack + 1, !cut_node);

                if(!move.is_cap()) {
                    int bonus = 0;
                    if(score <= alpha)
                        bonus = -history_bonus(new_depth);
                    else if(score >= beta)
                        bonus = history_bonus(new_depth);

                    history.update_conth(move, stack, bonus);
                }
            }
        }
        // full depth search if lmr was skipped
        else if(!pv_node || made_moves > 1) {
            score = -negamax<NodeType::NON_PV>(new_depth, -alpha - 1, -alpha, stack + 1, !cut_node);
        }

        // principal variation search
        if(pv_node && (made_moves == 1 || score > alpha))
            score = -negamax<NodeType::PV>(new_depth, -beta, -alpha, stack + 1);

        board.undo_move(move);

        assert(valid_score(score));

        if(is_limit_reached(depth))
            return 0;

        if(root_node) {
            int move_idx = root_moves.idx_of(move);
            assert(move_idx >= 0);
            RootMove *rm = &root_moves[move_idx];

            rm->nodes += total_nodes - start_nodes;
            rm->avg_score = valid_score(rm->avg_score) ? (rm->avg_score + score) / 2 : score;

            if(made_moves == 1 || score > alpha) {
                rm->set_score(score);
                rm->pv = (stack + 1)->pv;
                rm->pv[0] = move;
            } else {
                rm->set_score(-VALUE_INFINITE);
            }
        }

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;

                if(pv_node && !root_node)
                    update_pv(move, stack);
            }

            if(alpha >= beta) {
                history.update(board, best_move, quiets, noisy, stack,
                               depth + (best_score > beta + history_bonus_margin));
                break; // cut-off
            }
        }

        if(move != best_move) {
            auto &list = move.is_quiet() ? quiets : noisy;
            if(list.size() < 64)
                list.add(move);
        }
    }

    // check for mate/stalemate
    if(!made_moves) {
        if(stack->skipped)
            return alpha;
        else if(in_check)
            return stack->ply - VALUE_MATE;
        else
            return VALUE_DRAW;
    }

    if(pv_node)
        best_score = std::min(best_score, max_score);

    // store in transposition table
    Bound bound = EXACT_BOUND;
    if(best_score >= beta)
        bound = LOWER_BOUND;
    else if(best_score <= old_alpha)
        bound = UPPER_BOUND;

    if(!stack->skipped && !(root_node && multipv_idx)) {
        ent->store(     //
            hash,       //
            best_move,  //
            best_score, //
            raw_eval,   //
            bound,      //
            depth,      //
            stack->ply, //
            tt_pv       //
        );
    }

    // update correction histories
    if(!in_check                                                //
       && !(best_move && best_move.is_cap())                    //
       && valid_tt_score(best_score, stack->static_eval, bound) //
    ) {
        history.update_matcorr(board, raw_eval, best_score, depth);
        history.update_contcorr(raw_eval, best_score, depth, stack);
    }

    assert(valid_score(best_score));

    return best_score;
}

template <NodeType nt> //
Score Search::quiescence(int depth, Score alpha, Score beta, Stack *stack) {

    constexpr bool pv_node = (nt != NodeType::NON_PV);

    assert(stack->ply >= 0);
    assert(pv_node || (alpha == beta - 1));
    assert(valid_score(alpha + 1) && valid_score(beta - 1) && alpha < beta);

    if(pv_node)
        stack->pv.length = stack->ply;

    if(is_limit_reached(0))
        return 0;

    if(board.is_draw(stack->ply))
        return VALUE_DRAW;

    // check for upcoming repetition
    if(alpha < VALUE_DRAW && board.upcoming_repetition(stack->ply)) {
        alpha = VALUE_DRAW;
        if(alpha >= beta)
            return alpha;
    }

    const bool in_check = board.in_check();
    const U64 hash = board.get_hash();

    if(stack->ply >= MAX_PLY - 1)
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
        tt_score = ent->get_score(stack->ply);
        tt_eval = ent->get_eval();
        tt_pv |= ent->get_tt_pv();
    }

    if(!pv_node && valid_score(tt_score) && valid_tt_score(tt_score, beta, tt_bound)) {
        return tt_score;
    }

    if(in_check) {
        futility = raw_eval = stack->static_eval = VALUE_NONE;
    } else {
        raw_eval = valid_score(tt_eval) ? tt_eval : evaluate();
        best_score = stack->static_eval = adjust_eval(raw_eval, stack);
        futility = best_score + qfp_margin;

        if(valid_score(tt_score) && valid_tt_score(tt_score, best_score + 1, tt_bound)) {
            best_score = tt_score;
        }

        // stand pat
        if(best_score >= beta) {
            if(!is_decisive(best_score))
                best_score = (best_score + beta) / 2;

            if(!tt_hit) {
                ent->store(     //
                    hash,       //
                    NO_MOVE,    //
                    VALUE_NONE, // score
                    raw_eval,   //
                    NO_BOUND,   //
                    0,          // depth
                    stack->ply, //
                    false       // tt_pv
                );
            }

            return best_score;
        }

        if(best_score > alpha)
            alpha = best_score;
    }

    MovePicker mp(Q_SEARCH, board, history, stack, tt_move, depth >= -1);
    Move move = NO_MOVE;

    int made_moves = 0;
    while((move = mp.next()) != NO_MOVE) {
        if(!board.is_legal(move))
            continue;

        tt.prefetch(board.key_after(move));

        made_moves++;

        if(!is_loss(best_score)) {
            // if we are not losing while in check, we can skip quiet moves
            if(in_check && move.is_quiet())
                break;

            // futility pruning
            if(!in_check && !move.is_quiet() && futility <= alpha && !board.see(move, 1)) {
                best_score = std::max(best_score, futility);
                continue;
            }

            // see pruning
            if(!board.see(move, qsee_margin))
                continue;
        }

        total_nodes++;

        stack->move = move;
        stack->moved_piece = board.piece_at(move.from());
        stack->conth = &history.conth[move.is_cap()][stack->moved_piece][move.to()];

        board.make_move(move);
        Score score = -quiescence<nt>(depth - 1, -beta, -alpha, stack + 1);
        board.undo_move(move);

        assert(valid_score(score));

        if(is_limit_reached(0))
            return 0;

        if(score > best_score) {
            best_score = score;

            if(score > alpha) {
                alpha = score;
                best_move = move;

                if(pv_node)
                    update_pv(move, stack);
            }

            if(alpha >= beta)
                break; // cut-off
        }
    }

    if(board.in_check() && !made_moves)
        return stack->ply - VALUE_MATE;

    if(best_score >= beta && !is_decisive(best_score))
        best_score = (best_score + beta) / 2;

    Bound bound = (best_score >= beta) ? LOWER_BOUND : UPPER_BOUND;

    ent->store(     //
        hash,       //
        best_move,  //
        best_score, //
        raw_eval,   //
        bound,      //
        0,          // depth
        stack->ply, //
        tt_pv       //
    );

    assert(valid_score(best_score));

    return best_score;
}

Score Search::evaluate() {
    int eval = NNUE::nnue.forward(board);

    int material = 122 * board.count<PAWN>()     //
                   + 401 * board.count<KNIGHT>() //
                   + 427 * board.count<BISHOP>() //
                   + 663 * board.count<ROOK>()   //
                   + 1237 * board.count<QUEEN>();

    eval = (230 + material / 42) * eval / 440;

    return std::clamp(eval, int(-VALUE_MATE_IN_MAX_PLY), int(VALUE_MATE_IN_MAX_PLY));
}

Score Search::adjust_eval(int32_t eval, Stack *stack) const {
    eval = (eval * (200 - board.get_fmr())) / 200;
    eval += (history.get_matcorr(board) + history.get_contcorr(stack)) / 256;

    return std::clamp(                         //
        eval,                                  //
        int32_t(VALUE_TB_LOSS_IN_MAX_PLY + 1), //
        int32_t(VALUE_TB_WIN_IN_MAX_PLY - 1)   //
    );
}

unsigned int Search::probe_wdl() const {
    U64 w_occ = board.get_occupancy(WHITE);
    U64 b_occ = board.get_occupancy(BLACK);
    U64 occ = w_occ | b_occ;

    if(pop_count(occ) > signed(TB_LARGEST))
        return TB_RESULT_FAILED;

    Square ep_sq = board.get_state().ep_sq;

    return tb_probe_wdl(                       //
        w_occ,                                 //
        b_occ,                                 //
        board.get_piecebb(KING),               //
        board.get_piecebb(QUEEN),              //
        board.get_piecebb(ROOK),               //
        board.get_piecebb(BISHOP),             //
        board.get_piecebb(KNIGHT),             //
        board.get_piecebb(PAWN),               //
        board.get_fmr(),                       //
        board.get_state().castle_rights.any(), //
        valid_sq(ep_sq) ? ep_sq : 0,           //
        board.get_stm() == WHITE               //
    );
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

void Search::sort_rootmoves(int offset) {
    assert(offset >= 0);

    for(int i = offset; i < root_moves.size(); i++) {
        int best = i;
        for(int j = i + 1; j < root_moves.size(); j++)
            if(root_moves[j].get_score() > root_moves[i].get_score())
                best = j;

        if(best != i)
            std::swap(root_moves[i], root_moves[best]);
    }
}

bool Search::found_rootmove(const Move &move) {
    for(int i = multipv_idx; i < root_moves.size(); i++)
        if(root_moves[i] == move)
            return true;
    return false;
}

void Search::update_pv(const Move &move, Stack *stack) {
    stack->pv.length = (stack + 1)->pv.length;
    stack->pv[stack->ply] = move;

    for(int i = stack->ply + 1; i < (stack + 1)->pv.length; i++)
        stack->pv[i] = (stack + 1)->pv[i];
}

void Search::print_uci_info() const {
    if(id != 0)
        return; // only main thread prints UCI info

    const int64_t elapsed_time = tm.elapsed_time();
    const U64 total_nodes = threads.get_total_nodes();
    const Score score = root_moves[multipv_idx].get_score();
    const PVLine &pv_line = root_moves[multipv_idx].pv;

    std::cout << "info depth " << root_depth    //
              << " multipv " << multipv_idx + 1 //
              << " score ";                     //

    if(std::abs(score) >= VALUE_MATE_IN_MAX_PLY) {
        std::cout << "mate " << (VALUE_MATE - std::abs(score) + 1) / 2 * (score > 0 ? 1 : -1);
    } else {
        int material = board.count<PAWN>()         //
                       + 3 * board.count<KNIGHT>() //
                       + 3 * board.count<BISHOP>() //
                       + 5 * board.count<ROOK>()   //
                       + 9 * board.count<QUEEN>();

        double m = std::clamp(material, 17, 78) / 58.0;
        double d = -13.5 * pow(m, 3) + 40.9 * pow(m, 2) - 36.8 * m + 386.8;

        std::cout << "cp " << std::round(100 * int(score) / d);
    }

    std::cout << " nodes " << total_nodes                           //
              << " nps " << total_nodes * 1000 / (elapsed_time + 1) //
              << " tbhits " << threads.get_tb_hits()                //
              << " hashfull " << tt.hashfull()                      //
              << " time " << elapsed_time                           //
              << " pv " << root_moves[multipv_idx];

    // print rest of the pv
    for(int i = 1; i < pv_line.length; i++) {
        const Move &move = pv_line[i];
        if(!move)
            break;
        std::cout << " " << move;
    }

    std::cout << std::endl;
}

} // namespace Engine
