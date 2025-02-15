#include <iostream>
#include <cmath>

#include "search.h"
#include "tune.h"
#include "syzygy.h"
#include "threads.h"
#include "movepicker.h"

#include "../eval/eval.h"
#include "../uci.h"

namespace Astra
{
    int REDUCTIONS[MAX_PLY][MAX_MOVES];

    void initReductions()
    {
        REDUCTIONS[0][0] = 0;

        const double base = double(lmr_base) / 100.0;
        const double div_factor = double(lmr_div) / 100.0;

        for (int depth = 1; depth < MAX_PLY; depth++)
            for (int moves = 1; moves < MAX_MOVES; moves++)
                REDUCTIONS[depth][moves] = base + log(depth) * log(moves) / div_factor;
    }

    // search class

    Move Search::bestMove()
    {
        tm.start();

        if (use_tb)
        {
            const auto dtz = probeDTZ(board);
            if (dtz.second != NO_MOVE)
            {
                if (id == 0)
                    std::cout << "bestmove " << dtz.second << std::endl;

                return dtz.second;
            }
        }

        if (id == 0)
            tt.incrementAge();

        // init stack
        Stack stack[MAX_PLY + 6]; // +6 for history
        Stack *ss = stack + 6;

        for (int i = 0; i < MAX_PLY; i++)
            (ss + i)->ply = i;
        for (int i = 1; i <= 6; i++)
            (ss - i)->conth = &history.conth[0][WHITE_PAWN][a1];

        int stability = 0;
        Score prev_result = VALUE_NONE;

        Move bestmove = NO_MOVE;
        for (root_depth = 1; root_depth < MAX_PLY; root_depth++)
        {
            // reset selective depth
            sel_depth = 0;

            Score result = aspSearch(root_depth, prev_result, ss);

            if (isLimitReached(root_depth))
                break;

            if (id == 0 && root_depth >= 5 && limit.time.optimum)
            {
                int64_t elapsed = tm.elapsedTime();
                Move move = pv_table[0][0];

                // adjust time optimum based on stability
                stability = bestmove == move ? std::min(10, stability + 1) : 0;
                double stability_factor = 1.3115 - stability * 0.05329;

                // adjust time optimum based on last score
                double result_change_factor = 0.1788 + std::clamp(prev_result - result, 0, 62) * 0.003657;

                // adjust time optimum based on node count
                double not_best_nodes = 1.0 - double(move_nodes[move.from()][move.to()]) / double(nodes);
                double node_count_factor = not_best_nodes * 2.1223 + 0.4599;

                // check if we should stop
                if (elapsed > limit.time.optimum * stability_factor * result_change_factor * node_count_factor)
                    break;
            }

            bestmove = pv_table[0][0];
            prev_result = result;
        }

        // make sure to atleast have a best move
        if (bestmove == NO_MOVE)
            bestmove = pv_table[0][0];

        if (id == 0)
            std::cout << "bestmove " << bestmove << std::endl;

        return bestmove;
    }

    Score Search::aspSearch(int depth, Score prev_eval, Stack *ss)
    {
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;
        Score result = VALUE_NONE;

        int window = asp_window;
        if (depth >= asp_depth)
        {
            alpha = std::max(prev_eval - window, -int(VALUE_INFINITE));
            beta = std::min(prev_eval + window, int(VALUE_INFINITE));
        }

        int fail_high_count = 0;
        while (true)
        {
            if (alpha < -2000)
                alpha = -VALUE_INFINITE;
            if (beta > 2000)
                beta = VALUE_INFINITE;

            result = negamax(std::max(1, root_depth - fail_high_count), alpha, beta, ss, false);

            if (isLimitReached(depth))
                return result;
            else if (id == 0 && tm.elapsedTime() > 5000)
                UCI::printUciInfo(result, depth, tm.elapsedTime(), pv_table[0]);

            if (result <= alpha)
            {
                // if result is lower than alpha, than we can lower alpha for next iteration
                beta = (alpha + beta) / 2;
                alpha = std::max(alpha - window, -int(VALUE_INFINITE));
                fail_high_count = 0;
            }
            else if (result >= beta)
            {
                // if result is higher than beta, than we can raise beta for next iteration
                beta = std::min(beta + window, int(VALUE_INFINITE));
                if (std::abs(result) < VALUE_TB_WIN_IN_MAX_PLY)
                    fail_high_count++;
            }
            else
                break;

            window += window / 3.75;
        }

        if (id == 0)
            UCI::printUciInfo(result, depth, tm.elapsedTime(), pv_table[0]);

        return result;
    }

    Score Search::negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node, const Move skipped)
    {
        assert(alpha < beta);
        assert(ss->ply >= 0);

        // variables
        const bool root_node = ss->ply == 0;
        const bool pv_node = beta - alpha != 1;
        const bool in_check = board.inCheck();
        bool improving = false;

        const U64 hash = board.getHash();
        const Color stm = board.getTurn();

        const Score old_alpha = alpha;
        Score eval = ss->eval, raw_eval = eval;
        Score max_score = VALUE_MATE, best_score = -VALUE_MATE;

        pv_table[ss->ply].length = ss->ply;

        if (!root_node && alpha < VALUE_DRAW && board.hasUpcomingRepetition(ss->ply))
        {
            alpha = VALUE_DRAW;
            if (alpha >= beta)
                return alpha;
        }

        // do quiescence search if depth is less than 1
        if (depth <= 0)
            return qSearch(0, alpha, beta, ss);

        // selective depth
        if (pv_node && ss->ply > sel_depth)
            sel_depth = ss->ply; // heighest depth a pv node has reached

        // check for terminal state
        if (!root_node)
        {
            if (ss->ply >= MAX_PLY - 1)
                return Eval::evaluate(board);

            if (isLimitReached(depth))
                return 0;

            // mate distance pruning
            alpha = std::max(alpha, Score(ss->ply - VALUE_MATE));
            beta = std::min(beta, Score(VALUE_MATE - ss->ply - 1));
            if (alpha >= beta)
                return alpha;

            if (board.isDraw(ss->ply))
                return VALUE_DRAW;
        }

        // reset killer
        (ss + 1)->killer = NO_MOVE;

        // look up in transposition table
        bool tt_hit = false;
        TTEntry *ent = !skipped ? tt.lookup(hash, &tt_hit) : nullptr;
        Move tt_move = tt_hit ? ent->getMove() : NO_MOVE;
        Bound tt_bound = tt_hit ? ent->getBound() : NO_BOUND;
        Score tt_score = tt_hit ? ent->getScore(ss->ply) : VALUE_NONE;
        Score tt_eval = tt_hit ? ent->eval : VALUE_NONE;
        int tt_depth = tt_hit * ent->depth;
        bool tt_pv = pv_node || (tt_hit ? ent->getTTPv() : false);

        // clang-format off
        if (!pv_node && tt_depth >= depth && tt_score != VALUE_NONE && board.halfMoveClock() < 85 
            && (tt_bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND)))
            return tt_score;
        // clang-format on

        // tablebase probing
        if (use_tb && !root_node)
        {
            Score tb_score = probeWDL(board);

            if (tb_score != VALUE_NONE)
            {
                Bound bound = NO_BOUND;
                tb_hits++;

                // clang-format off
                tb_score = tb_score == VALUE_TB_WIN ? VALUE_MATE - ss->ply : tb_score == -VALUE_TB_WIN ? -VALUE_MATE + ss->ply + 1 : 0;
                bound = tb_score == VALUE_TB_WIN ? LOWER_BOUND : tb_score == -VALUE_TB_WIN ? UPPER_BOUND : EXACT_BOUND;
                // clang-format on

                if (bound == EXACT_BOUND || (bound == LOWER_BOUND && tb_score >= beta) || (bound == UPPER_BOUND && tb_score <= alpha))
                {
                    ent->store(hash, NO_MOVE, tb_score, tt_eval, bound, depth, ss->ply, tt_pv);
                    return tb_score;
                }

                if (pv_node)
                {
                    if (bound == LOWER_BOUND)
                    {
                        best_score = tb_score;
                        alpha = std::max(alpha, best_score);
                    }
                    else
                        max_score = tb_score;
                }
            }
        }

        // set eval and static eval
        if (in_check)
            raw_eval = eval = ss->eval = VALUE_NONE;
        else
        {
            // use tt score for better evaluation
            if (tt_hit)
            {
                raw_eval = tt_eval == VALUE_NONE ? Eval::evaluate(board) : tt_eval;
                eval = ss->eval = adjustEval(board, ss, raw_eval);

                if (tt_score != VALUE_NONE && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else if (!skipped)
            {
                raw_eval = Eval::evaluate(board);
                eval = ss->eval = adjustEval(board, ss, raw_eval);
                ent->store(hash, NO_MOVE, VALUE_NONE, raw_eval, NO_BOUND, 0, ss->ply, tt_pv);
            }

            // check for improvement
            if ((ss - 2)->eval != VALUE_NONE)
                improving = ss->eval > (ss - 2)->eval;
            else if ((ss - 4)->eval != VALUE_NONE)
                improving = ss->eval > (ss - 4)->eval;
        }

        // update quiet history
        Move prev_move = (ss - 1)->curr_move;
        if (!prev_move && !isCap(prev_move) && (ss - 1)->eval != VALUE_NONE)
        {
            int bonus = std::clamp(-5 * (ss->eval + (ss - 1)->eval), -80, 150);
            history.updateQuietHistory(~stm, prev_move, bonus);
        }

        // internal iterative reduction
        if (!in_check && !tt_move && depth >= 4 && (pv_node || cut_node))
            depth--;

        // only use pruning when not in check and pv node
        if (!in_check && !pv_node && !skipped)
        {
            // reverse futility pruning
            int rfp_margin = std::max(rfp_depth_mult * (depth - (improving && !board.oppHasGoodCaptures())), 20);
            if (depth <= rfp_depth && eval < VALUE_TB_WIN_IN_MAX_PLY && eval - rfp_margin >= beta)
                return (eval + beta) / 2;

            // razoring
            if (depth < 5 && eval + rzr_depth_mult * depth < alpha)
            {
                Score score = qSearch(0, alpha, beta, ss);
                if (score <= alpha)
                    return score;
            }

            // null move pruning
            // clang-format off
            if (depth >= 4 && eval >= beta && ss->eval + nmp_depth_mult * depth - nmp_base >= beta
                && board.nonPawnMat(stm) && (ss - 1)->curr_move != NULL_MOVE && beta > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                // clang-format on
                int R = 4 + depth / nmp_depth_div + std::min(int(nmp_min), (eval - beta) / nmp_div);

                ss->curr_move = NULL_MOVE;
                ss->moved_piece = NO_PIECE;
                ss->conth = &history.conth[0][WHITE_PAWN][a1]; // put null move to quiet moves

                board.makeNullMove();
                Score score = -negamax(depth - R, -beta, -beta + 1, ss + 1, !cut_node);
                board.unmakeNullMove();

                if (score >= beta)
                {
                    // don't return unproven mate scores
                    if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                        score = beta;
                    return score;
                }
            }

            // probcut
            // clang-format off
            int beta_cut = beta + probcut_margin;
            if (depth > 4 && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY 
                && !(tt_depth >= depth - 3 && tt_score != VALUE_NONE && tt_score < beta_cut))
            {
                // clang-format on
                MovePicker mp(PC_SEARCH, board, history, ss, tt_move);
                mp.see_cutoff = beta_cut > eval;

                Move move = NO_MOVE;
                while ((move = mp.nextMove()) != NO_MOVE)
                {
                    if (!board.isLegal(move))
                        continue;

                    // prefetch tt entry
                    tt.prefetch(board.keyAfter(move));

                    nodes++;
                    ss->curr_move = move;
                    ss->moved_piece = board.pieceAt(move.from());
                    ss->conth = &history.conth[isCap(move)][ss->moved_piece][move.to()];

                    board.makeMove(move, true);
                    Score score = -qSearch(0, -beta_cut, -beta_cut + 1, ss + 1);

                    if (score >= beta_cut)
                        score = -negamax(depth - 4, -beta_cut, -beta_cut + 1, ss + 1, !cut_node);

                    board.unmakeMove(move);

                    if (score >= beta_cut)
                    {
                        ent->store(hash, move, score, ss->eval, LOWER_BOUND, depth - 3, ss->ply, tt_pv);
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

        while ((move = mp.nextMove()) != NO_MOVE)
        {
            if (move == skipped || !board.isLegal(move))
                continue;

            U64 start_nodes = nodes;

            // prefetch tt entry
            tt.prefetch(board.keyAfter(move));

            made_moves++;

            int history_score = isCap(move) ? history.getCapHistory(board, move) : history.getQuietHistory(board, ss, move);

            if (!root_node && best_score > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                // late move pruning
                if (!pv_node && q_count > (3 + depth * depth) / (2 - improving))
                    mp.skip_quiets = true;

                if (!isCap(move) && move.type() != PQ_QUEEN)
                {
                    // history pruning
                    if (history_score < -hp_margin * depth && depth <= hp_depth)
                        continue;

                    // futility pruning
                    if (!in_check && depth <= fp_depth && eval + fp_base + depth * fp_mult <= alpha)
                        mp.skip_quiets = true;
                }

                // see pruning
                int see_margin = isCap(move) ? -see_cap_margin : -see_quiet_margin;
                int see_depth = isCap(move) ? see_cap_depth : see_quiet_depth;
                if (depth <= see_depth && !board.see(move, depth * see_margin))
                    continue;
            }

            if (isCap(move) && c_count < 32)
                c_moves[c_count++] = move;
            else if (q_count < 64)
                q_moves[q_count++] = move;

            // print current move information
            if (id == 0 && root_node && tm.elapsedTime() > 5000 && !threads.isStopped())
                std::cout << "info depth " << depth << " currmove " << move << " currmovenumber " << made_moves << std::endl;

            int extension = 0;

            // singular extensions
            // clang-format off
            if (!root_node && depth >= 6 && ss->ply < 2 * root_depth && !skipped && tt_move == move 
                && tt_depth >= depth - 3 && (tt_bound & LOWER_BOUND) && std::abs(tt_score) < VALUE_TB_WIN_IN_MAX_PLY)
            {
                // clang-format on
                Score sbeta = tt_score - 3 * depth;
                int sdepth = (depth - 1) / 2;

                Score score = negamax(sdepth, sbeta - 1, sbeta, ss, cut_node, move);

                if (score < sbeta)
                {
                    // if we didn't find a better move, then extend
                    if (!pv_node && score < sbeta - 14)
                        extension = 2 + (!isCap(move) && !isProm(move) && score < sbeta - ext_margin);
                    else
                        extension = 1;
                }
                else if (sbeta >= beta)
                    return sbeta;
                else if (tt_score >= beta)
                    extension = -2 + pv_node;
                else if (cut_node)
                    extension = -2;
                else if (tt_score <= alpha)
                    extension = -1;
            }

            int new_depth = depth - 1 + extension;

            nodes++;

            ss->curr_move = move;
            ss->moved_piece = board.pieceAt(move.from());
            ss->conth = &history.conth[isCap(move)][ss->moved_piece][move.to()];

            board.makeMove(move, true);

            Score score = VALUE_NONE;

            // late move reductions
            if (depth > 1 && made_moves > 2 + 2 * root_node && (!pv_node || !isCap(move)))
            {
                int r = REDUCTIONS[depth][made_moves];
                // increase when not improving
                r += !improving;
                // increase when expected to fail high
                r += 2 * cut_node;
                // decrease when in pv node
                r -= (pv_node + tt_pv);
                // decrease when move gives check
                r -= board.inCheck();
                // decrease/increase based on history score
                r -= history_score / (isCap(move) ? hp_cdiv : hp_qdiv);
                // decrease when tt depth is at least current depth
                r -= (tt_depth >= depth);

                int lmr_depth = std::clamp(new_depth - r, 1, new_depth + 1);

                score = -negamax(lmr_depth, -alpha - 1, -alpha, ss + 1, true);

                // if late move reduction failed high and we actually reduced, do a research
                if (score > alpha && lmr_depth < new_depth)
                {
                    score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1, !cut_node);

                    if (!isCap(move))
                    {
                        // clang-format off
                        int bonus = score <= alpha ? -historyBonus(new_depth) : score >= beta ? historyBonus(new_depth) : 0;
                        // clang-format on
                        history.updateContH(move, ss, bonus);
                    }
                }
            }
            else if (!pv_node || made_moves > 1)
                // full-depth search if lmr was skipped
                score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1, !cut_node);

            // principal variation search
            if (pv_node && ((score > alpha && (score < beta || root_node)) || made_moves == 1))
                score = -negamax(new_depth, -beta, -alpha, ss + 1, false);

            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            if (isLimitReached(depth))
                return 0;

            if (root_node)
                move_nodes[move.from()][move.to()] = nodes - start_nodes;

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;

                    // update pv node when in pv node
                    if (id == 0 && pv_node)
                        updatePv(move, ss->ply);
                }

                if (alpha >= beta)
                {
                    history.update(board, move, ss, q_moves, q_count, c_moves, c_count, depth + (best_score > beta + hbonus_margin));
                    break; // cut-off
                }
            }
        }

        // check for mate and stalemate
        if (made_moves == 0)
            // clang-format off
            return skipped != NO_MOVE ? alpha : in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;
        // clang-format on

        best_score = std::min(best_score, max_score);

        // store in transposition table
        // clang-format off
        Bound bound = best_score >= beta ? LOWER_BOUND : best_score <= old_alpha ? UPPER_BOUND : EXACT_BOUND;
        // clang-format on
        if (!skipped)
            ent->store(hash, best_move, best_score, raw_eval, bound, depth, ss->ply, tt_pv);

        // update correction histories
        if (!in_check && !isCap(best_move) && (bound & (best_score >= raw_eval ? LOWER_BOUND : UPPER_BOUND)))
        {
            history.updateMaterialCorr(board, raw_eval, best_score, depth);
            history.updateContCorr(raw_eval, best_score, depth, ss);
        }

        return best_score;
    }

    Score Search::qSearch(int depth, Score alpha, Score beta, Stack *ss)
    {
        assert(alpha < beta);

        if (isLimitReached(1))
            return 0;

        if (alpha < VALUE_DRAW && board.hasUpcomingRepetition(ss->ply))
        {
            alpha = VALUE_DRAW;
            if (alpha >= beta)
                return alpha;
        }

        if (board.isDraw(ss->ply))
            return VALUE_DRAW;

        if (ss->ply >= MAX_PLY - 1)
            return Eval::evaluate(board);

        // variables
        const bool pv_node = beta - alpha != 1;
        const bool in_check = board.inCheck();

        const U64 hash = board.getHash();

        Score best_score = -VALUE_MATE + ss->ply;
        Score eval = ss->eval;
        Score raw_eval = eval;
        Score futility = -VALUE_MATE;

        Move best_move = NO_MOVE;

        // look up in transposition table
        bool tt_hit = false;
        TTEntry *ent = tt.lookup(hash, &tt_hit);
        Move tt_move = tt_hit ? ent->getMove() : NO_MOVE;
        Bound tt_bound = tt_hit ? ent->getBound() : NO_BOUND;
        Score tt_score = tt_hit ? ent->getScore(ss->ply) : VALUE_NONE;
        Score tt_eval = tt_hit ? ent->eval : VALUE_NONE;
        bool tt_pv = pv_node || (tt_hit ? ent->getTTPv() : false);

        if (!pv_node && tt_score != VALUE_NONE && (tt_bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND)))
            return tt_score;

        // set eval and static eval
        if (in_check)
            raw_eval = eval = ss->eval = VALUE_NONE;
        else
        {
            // use tt score for better evaluation
            if (tt_hit)
            {
                raw_eval = tt_eval == VALUE_NONE ? Eval::evaluate(board) : tt_eval;
                eval = ss->eval = adjustEval(board, ss, raw_eval);

                if (tt_score != VALUE_NONE && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else
            {
                raw_eval = Eval::evaluate(board);
                eval = ss->eval = adjustEval(board, ss, raw_eval);
                ent->store(hash, NO_MOVE, VALUE_NONE, raw_eval, NO_BOUND, 0, ss->ply, tt_pv);
            }

            best_score = eval;
            futility = eval + qfp_margin;

            // stand pat
            if (best_score >= beta)
                return best_score;
            if (best_score > alpha)
                alpha = best_score;
        }

        MovePicker mp(Q_SEARCH, board, history, ss, tt_move, depth >= -1);

        Move move;

        int made_moves = 0;
        while ((move = mp.nextMove()) != NO_MOVE)
        {
            if (!board.isLegal(move))
                continue;

            // prefetch tt entry
            tt.prefetch(board.keyAfter(move));

            made_moves++;

            if (best_score > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                if (!in_check && futility <= alpha && isCap(move) && !board.see(move, 1))
                {
                    best_score = std::max(best_score, futility);
                    continue;
                }

                // see pruning
                if (!board.see(move, 0))
                    continue;
            }

            nodes++;

            ss->curr_move = move;
            ss->moved_piece = board.pieceAt(move.from());
            ss->conth = &history.conth[isCap(move)][ss->moved_piece][move.to()];

            board.makeMove(move, true);
            Score score = -qSearch(depth - 1, -beta, -alpha, ss + 1);
            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            // update the best score
            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;
                }

                if (alpha >= beta)
                    break; // cut-off
            }

            if (best_score > -VALUE_TB_WIN_IN_MAX_PLY)
                mp.skip_quiets = true;
        }

        if (in_check && made_moves == 0)
            return -VALUE_MATE + ss->ply;

        if (best_score >= beta && abs(best_score) < VALUE_TB_WIN_IN_MAX_PLY)
            best_score = (best_score + beta) / 2;

        // store in transposition table
        Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND; // no exact bound in qsearch
        ent->store(hash, best_move, best_score, raw_eval, bound, 0, ss->ply, tt_pv);

        return best_score;
    }

    bool Search::isLimitReached(int depth) const
    {
        if (limit.infinite)
            return false;
        if (threads.isStopped())
            return true;
        if (limit.nodes != 0 && nodes >= limit.nodes)
            return true;
        if (depth > limit.depth)
            return true;
        if (limit.time.max != 0 && tm.elapsedTime() >= limit.time.max)
            return true;

        return false;
    }

    void Search::updatePv(Move move, int ply)
    {
        pv_table[ply][ply] = move;
        for (int next_ply = ply + 1; next_ply < pv_table[ply + 1].length; next_ply++)
            pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];
        pv_table[ply].length = pv_table[ply + 1].length;
    }

} // namespace Astra
