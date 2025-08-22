#include <algorithm>
#include <cmath>

#include "../chess/movegen.h"
#include "../nnue/nnue.h"

#include "movepicker.h"
#include "search.h"
#include "syzygy.h"
#include "threads.h"
#include "tune_params.h"

namespace Search {

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

    MoveList<> legals;
    legals.gen<LEGALS>(board);

    if(legals.size() == 0) {
        std::cout << "Position has no legal moves" << std::endl;
        return;
    }

    for(int i = 0; i < legals.size(); i++)
        rootmoves.add({legals[i], 0, 0, 0, {}});

    const int multipv_size = std::min(limits.multipv, rootmoves.size());

    if(id == 0)
        tt.increment();

    Move best_move = NO_MOVE;
    Move prev_best_move = NO_MOVE;

    // init stack
    Stack stack[MAX_PLY + 7]; // +6 for continuation history, +1 for safety
    Stack *s = &stack[6];
    for(int i = 0; i < MAX_PLY + 7; i++) {
        stack[i].ply = i - 6;
        stack[i].conth = &history.conth[0][NO_PIECE][NO_SQUARE];
    }

    int stability = 0;
    Score scores[MAX_PLY];

    for(root_depth = 1; root_depth <= MAX_PLY; root_depth++) {
        for(multipv_idx = 0; multipv_idx < multipv_size; multipv_idx++)
            aspiration(root_depth, s);

        if(is_limit_reached(root_depth))
            break;

        // print final pv info
        for(multipv_idx = 0; multipv_idx < multipv_size; multipv_idx++)
            print_uci_info();

        Score score = rootmoves[0].score;
        best_move = rootmoves[0].move;

        if(best_move == prev_best_move)
            stability = std::min(stability + 1, tm_stability_max);
        else
            stability = 0;

        if(id == 0                                          //
           && root_depth >= 5                               //
           && tm.should_stop(                               //
                  limits,                                   //
                  stability,                                //
                  scores[root_depth - 1] - score,           //
                  scores[root_depth - 3] - score,           //
                  rootmoves[0].nodes / double(total_nodes)) //
        ) {
            break;
        }

        prev_best_move = best_move;
        scores[root_depth] = score;
    }

    if(id == 0)
        std::cout << "bestmove " << best_move << std::endl;

    threads.stop();
}

Score Search::aspiration(int depth, Stack *s) {
    Score score;
    Score alpha = -VALUE_INFINITE;
    Score beta = VALUE_INFINITE;

    int delta = asp_delta;
    if(depth >= asp_depth) {
        Score avg_score = rootmoves[multipv_idx].avg_score;
        alpha = std::max(avg_score - delta, int(-VALUE_INFINITE));
        beta = std::min(avg_score + delta, int(VALUE_INFINITE));
    }

    int fail_high_count = 0;
    while(true) {
        score = negamax<NodeType::ROOT>(std::max(1, root_depth - fail_high_count), alpha, beta, s, false);

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
Score Search::negamax(int depth, Score alpha, Score beta, Stack *s, bool cut_node) {

    constexpr bool root_node = (nt == NodeType::ROOT);
    constexpr bool pv_node = (nt != NodeType::NON_PV);

    assert(s->ply >= 0);
    assert(!(pv_node && cut_node));
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
    Score max_score = VALUE_INFINITE;

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

        // check for upcoming repetition
        if(alpha < VALUE_DRAW && board.has_upcoming_repetition(s->ply)) {
            alpha = VALUE_DRAW;
            if(alpha >= beta)
                return alpha;
        }

        // mate distance pruning
        alpha = std::max(alpha, Score(s->ply - VALUE_MATE));
        beta = std::min(beta, Score(VALUE_MATE - s->ply - 1));
        if(alpha >= beta)
            return alpha;
    }

    // look up in transposition table
    bool tt_hit = false;
    TTEntry *ent = !s->skipped ? tt.lookup(hash, &tt_hit) : nullptr;

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

    // tablebase probing
    if(use_tb && !root_node && !s->skipped) {
        auto tb_result = probe_wdl(board);

        if(tb_result != TB_RESULT_FAILED) {
            Bound tb_bound;
            Score tb_score;
            tb_hits++;

            if(tb_result == TB_LOSS) {
                tb_score = s->ply - VALUE_TB;
                tb_bound = UPPER_BOUND;
            } else if(tb_result == TB_WIN) {
                tb_score = VALUE_TB - s->ply;
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
                    s->ply,     //
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
        raw_eval = eval = s->static_eval = VALUE_NONE;
        goto movesloop;
    } else if(s->skipped)
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

    if(valid_score((s - 2)->static_eval))
        improving = s->static_eval > (s - 2)->static_eval;
    else if(valid_score((s - 4)->static_eval))
        improving = s->static_eval > (s - 4)->static_eval;

    // update quiet history
    if((s - 1)->move                        //
       && !(s - 1)->move.is_cap()           //
       && valid_score((s - 1)->static_eval) //
    ) {
        int bonus = std::clamp(                                           //
            static_h_mult * (s->static_eval + (s - 1)->static_eval) / 16, //
            int(-static_h_min),                                           //
            int(static_h_max)                                             //
        );

        history.update_hh(~stm, (s - 1)->move, bonus);
    }

    // razoring
    if(!pv_node                                 //
       && !is_win(alpha)                        //
       && depth < rzr_depth                     //
       && eval + rzr_depth_mult * depth < alpha //
    ) {
        Score score = quiescence<NodeType::NON_PV>(0, alpha, beta, s);
        if(score <= alpha)
            return score;
    }

    // reverse futility pruning
    if(!pv_node                                                                    //
       && !s->skipped                                                              //
       && !is_win(eval)                                                            //
       && !is_loss(beta)                                                           //
       && depth < rfp_depth                                                        //
       && eval - (rfp_depth_mult * depth - rfp_improving_mult * improving) >= beta //
    ) {
        return (eval + beta) / 2;
    }

    // null move pruning
    if(!pv_node                                                      //
       && depth >= 4                                                 //
       && !s->skipped                                                //
       && eval >= beta                                               //
       && !is_loss(beta)                                             //
       && board.nonpawnmat(stm)                                      //
       && (s - 1)->move != NULL_MOVE                                 //
       && s->static_eval + nmp_depth_mult * depth - nmp_base >= beta //
    ) {
        int R = nmp_rbase                //
                + depth / nmp_rdepth_div //
                + std::min(int(nmp_rmin), (eval - beta) / nmp_eval_div);

        s->move = NULL_MOVE;
        s->moved_piece = NO_PIECE;
        s->conth = &history.conth[0][NO_PIECE][NO_SQUARE];

        board.make_nullmove();
        Score score = -negamax<NodeType::NON_PV>(depth - R, -beta, -beta + 1, s + 1, !cut_node);
        board.unmake_nullmove();

        if(score >= beta) {
            // don't return unproven mate scores
            if(is_win(score))
                score = beta;
            return score;
        }
    }

    // internal iterative reduction
    if(!s->skipped && depth >= 4 && !tt_move && (pv_node || cut_node)) {
        depth--;
    }

    // probcut
    beta_cut = beta + prob_cut_margin;
    if(!pv_node                                           //
       && depth > 4                                       //
       && !s->skipped                                     //
       && !is_decisive(beta)                              //
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
                score = -negamax<NodeType::NON_PV>(depth - 4, -beta_cut, -beta_cut + 1, s + 1, !cut_node);

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
        if(root_node && !found_rootmove(move))
            continue;

        tt.prefetch(board.key_after(move));

        U64 start_nodes = total_nodes;

        made_moves++;

        int history_score = move.is_quiet() ? history.get_qh(board, move, s) //
                                            : history.get_nh(board, move);

        if(!root_node && !is_loss(best_score)) {
            const int lmr_depth = std::max(0, depth - REDUCTIONS[depth][made_moves] + history_score / hp_div);

            // late move pruning
            if(q_count > (3 + depth * depth) / (2 - improving))
                mp.skip_quiets();

            // futility pruning
            if(!move.is_cap()                                             //
               && lmr_depth < fp_depth                                    //
               && s->static_eval + fp_base + lmr_depth * fp_mult <= alpha //
            ) {
                mp.skip_quiets();
            }

            // history pruning
            if(!move.is_cap() && lmr_depth < hp_depth && history_score < -hp_depth_mult * depth) {
                mp.skip_quiets();
                continue;
            }

            // see pruning
            int see_depth = move.is_cap() ? depth : lmr_depth * lmr_depth;
            int see_margin = move.is_cap() ? see_cap_margin : see_quiet_margin;
            if(!board.see(move, see_depth * -see_margin))
                continue;
        }

        // singular extensions
        int extensions = 0;

        if(!root_node                 //
           && depth >= 6              //
           && !s->skipped             //
           && move == tt_move         //
           && tt_depth >= depth - 3   //
           && valid_score(tt_score)   //
           && !is_decisive(tt_score)  //
           && tt_bound & LOWER_BOUND  //
           && s->ply < 2 * root_depth //
        ) {
            Score sbeta = tt_score - 6 * depth / 8;
            s->skipped = move;
            Score score = negamax<NodeType::NON_PV>((depth - 1) / 2, sbeta - 1, sbeta, s, cut_node);
            s->skipped = NO_MOVE;

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

        s->move = move;
        s->moved_piece = board.piece_at(move.from());
        s->conth = &history.conth[move.is_cap()][s->moved_piece][move.to()];

        if(move.is_cap() && c_count < 64)
            c_moves[c_count++] = move;
        else if(q_count < 64)
            q_moves[q_count++] = move;

        total_nodes++;

        int new_depth = depth - 1 + extensions;

        board.make_move(move);

        Score score = VALUE_NONE;

        // late move reductions
        if(depth >= 2 && made_moves >= lmr_min_moves && (!tt_pv || !move.is_cap())) {
            int r = REDUCTIONS[depth][made_moves];

            r += !improving;

            r += 2 * cut_node;

            r += tt_move ? tt_move.is_cap() : 0;

            r -= tt_pv;

            r -= board.in_check();

            r -= tt_depth >= depth;

            r -= history_score / (move.is_cap() ? hp_cdiv : hp_qdiv);

            const int lmr_depth = std::clamp(new_depth - r, 1, new_depth + 1);

            score = -negamax<NodeType::NON_PV>(lmr_depth, -alpha - 1, -alpha, s + 1, true);

            if(score > alpha && lmr_depth < new_depth) {
                new_depth += (score > best_score + zws_margin);
                new_depth -= (score < best_score + new_depth);

                if(lmr_depth < new_depth)
                    score = -negamax<NodeType::NON_PV>(new_depth, -alpha - 1, -alpha, s + 1, !cut_node);

                if(!move.is_cap()) {
                    int bonus = 0;
                    if(score <= alpha)
                        bonus = -history_bonus(new_depth);
                    else if(score >= beta)
                        bonus = history_bonus(new_depth);

                    history.update_conth(move, s, bonus);
                }
            }
        } else if(!pv_node || made_moves > 1) {
            score = -negamax<NodeType::NON_PV>(new_depth, -alpha - 1, -alpha, s + 1, !cut_node);
        }

        // principal variation search
        if(pv_node && (score > alpha || made_moves == 1))
            score = -negamax<NodeType::PV>(new_depth, -beta, -alpha, s + 1, false);

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

            rm->nodes += total_nodes - start_nodes;
            rm->avg_score = !valid_score(rm->avg_score) ? score : (rm->avg_score + score) / 2;

            if(made_moves == 1 || score > alpha) {
                rm->score = score;
                // copy pv line
                rm->pv[0] = move;

                rm->pv.length = pv_table[s->ply + 1].length;
                for(int i = 1; i < rm->pv.length; i++)
                    rm->pv[i] = pv_table[s->ply + 1][i];
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
                    update_pv(move, s->ply);
            }

            if(alpha >= beta) {
                history.update(                                 //
                    board,                                      //
                    move, s,                                    //
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
        if(s->skipped)
            return alpha;
        else if(in_check)
            return s->ply - VALUE_MATE;
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

    if(!s->skipped && (!root_node || multipv_idx == 0)) {
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
       && (!best_move || !best_move.is_cap())               //
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

    // check for upcoming repetition
    if(alpha < VALUE_DRAW && board.has_upcoming_repetition(s->ply)) {
        alpha = VALUE_DRAW;
        if(alpha >= beta)
            return alpha;
    }

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

            if(!board.see(move, qsee_margin))
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

void Search::update_pv(const Move &move, int ply) {
    pv_table[ply][ply] = move;
    for(int nply = ply + 1; nply < pv_table[ply + 1].length; nply++)
        pv_table[ply][nply] = pv_table[ply + 1][nply];
    pv_table[ply].length = pv_table[ply + 1].length;
}

void Search::print_uci_info() const {
    if(id != 0)
        return; // only main thread prints UCI info

    const int64_t elapsed_time = tm.elapsed_time();
    const U64 total_nodes = threads.get_total_nodes();
    const Score score = rootmoves[multipv_idx].score;
    const PVLine &pv_line = rootmoves[multipv_idx].pv;

    std::cout << "info depth " << root_depth    //
              << " multipv " << multipv_idx + 1 //
              << " score ";                     //

    if(std::abs(score) >= VALUE_MATE_IN_MAX_PLY)
        std::cout << "mate " << (VALUE_MATE - std::abs(score) + 1) / 2 * (score > 0 ? 1 : -1);
    else
        std::cout << "cp " << Score(score / 2.5); // normalize

    std::cout << " nodes " << total_nodes                           //
              << " nps " << total_nodes * 1000 / (elapsed_time + 1) //
              << " tbhits " << threads.get_tb_hits()                //
              << " hashfull " << tt.hashfull()                      //
              << " time " << elapsed_time                           //
              << " pv " << rootmoves[multipv_idx].move;

    // print rest of the pv
    for(int i = 1; i < pv_line.length; i++)
        std::cout << " " << pv_line[i];

    std::cout << std::endl;
}

} // namespace Search
