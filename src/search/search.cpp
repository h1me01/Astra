#include <cmath>

#include "search.h"
#include "tune.h"
#include "syzygy.h"
#include "threads.h"
#include "movepicker.h"
#include "../net/nnue.h"
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

    Search::Search(const std::string &fen) : board(fen)
    {
        tt.clear();
    }

    Move Search::bestMove()
    {
        tm.start();

        MoveList<> legals;
        legals.gen<LEGALS>(board);

        for (int i = 0; i < legals.size(); i++)
            root_moves.add({legals[i], 0, 0, 0, 0, {}});

        const int multipv_size = std::min(limit.multipv, root_moves.size());

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
        Stack *ss = &stack[6];
        for (int i = 0; i < MAX_PLY + 6; i++)
        {
            stack[i].ply = i - 6;
            stack[i].eval = VALUE_NONE;
            stack[i].killer = NO_MOVE;
            stack[i].moved_piece = NO_PIECE;
            stack[i].curr_move = NO_MOVE;
            stack[i].conth = &history.conth[0][WHITE_PAWN][a1];
        }

        int stability = 0;
        Score prev_result = VALUE_NONE;
        Score scores[MAX_PLY];

        Move bestmove = NO_MOVE, prev_bestmove = NO_MOVE;
        for (root_depth = 1; root_depth < MAX_PLY; root_depth++)
        {
            // reset selective depth
            seldepth = 0;

            for (multipv_idx = 0; multipv_idx < multipv_size; multipv_idx++)
                aspSearch(root_depth, ss);

            if (isLimitReached(root_depth))
                break;

            // print final pv info if main thread
            for (multipv_idx = 0; multipv_idx < multipv_size && id == 0; multipv_idx++)
                printUciInfo();

            Score result = root_moves[0].score;
            bestmove = root_moves[0].move;

            if (id == 0 && root_depth >= 5 && limit.time.optimum)
            {
                int64_t elapsed = tm.elapsedTime();

                // adjust time optimum based on stability
                stability = bestmove == prev_bestmove ? std::min(9, stability + 1) : 0;
                double stability_factor = (stability_base / 100.0) - stability * (stability_mult / 100.0);

                // adjust time optimum based on last score
                // clang-format off
                double result_change_factor = (results_base / 100.0) 
                                            + (results_mult1 / 1000.0) * (prev_result - result) 
                                            + (results_mult2 / 1000.0) * (scores[root_depth - 3] - result);
                // clang-format on

                result_change_factor = std::clamp(result_change_factor, results_min / 100.0, results_max / 100.0);

                // adjust time optimum based on node count
                double not_best_nodes = 1.0 - double(root_moves[0].nodes / double(nodes));
                double node_count_factor = not_best_nodes * (node_mult / 100.0) + (node_base / 100.0);

                // check if we should stop
                if (elapsed > limit.time.optimum * stability_factor * result_change_factor * node_count_factor)
                    break;
            }

            prev_bestmove = bestmove;
            prev_result = result;
            scores[root_depth] = result;
        }

        // make sure to atleast have a best move
        if (bestmove == NO_MOVE)
            bestmove = root_moves[0].move;

        if (id == 0)
            std::cout << "bestmove " << bestmove << std::endl;

        return bestmove;
    }

    Score Search::aspSearch(int depth, Stack *ss)
    {
        Score result;
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;

        int window = 9;
        if (depth >= 6)
        {
            Score avg_score = root_moves[multipv_idx].avg_score;
            alpha = std::max(avg_score - window, -int(VALUE_INFINITE));
            beta = std::min(avg_score + window, int(VALUE_INFINITE));
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
                return 0;
            if (id == 0 && limit.multipv == 1 && tm.elapsedTime() > 5000)
                printUciInfo();

            sortRootMoves(multipv_idx);

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

            window += window / 3;
        }

        sortRootMoves(0);

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

        if (pv_node)
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
        if (pv_node && ss->ply > seldepth)
            seldepth = ss->ply; // heighest depth a pv node has reached

        // check for terminal state
        if (!root_node)
        {
            if (ss->ply >= MAX_PLY - 1)
                return evaluate();
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

        // look up in transposition table
        bool tt_hit = false;
        TTEntry *ent = !skipped ? tt.lookup(hash, &tt_hit) : nullptr;
        Move tt_move = tt_hit ? root_node ? root_moves[multipv_idx].move : ent->getMove() : NO_MOVE;
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
                raw_eval = tt_eval == VALUE_NONE ? evaluate() : tt_eval;
                eval = ss->eval = adjustEval(ss, raw_eval);

                if (tt_score != VALUE_NONE && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else if (!skipped)
            {
                raw_eval = evaluate();
                eval = ss->eval = adjustEval(ss, raw_eval);
                ent->store(hash, NO_MOVE, VALUE_NONE, raw_eval, NO_BOUND, 0, ss->ply, tt_pv);
            }

            // check for improvement
            if ((ss - 2)->eval != VALUE_NONE)
                improving = ss->eval > (ss - 2)->eval;
            else if ((ss - 4)->eval != VALUE_NONE)
                improving = ss->eval > (ss - 4)->eval;
        }

        // reset killer
        (ss + 1)->killer = NO_MOVE;

        // update quiet history
        Move prev_move = (ss - 1)->curr_move;
        if (!skipped && prev_move != NO_MOVE && !isCap(prev_move) && (ss - 1)->eval != VALUE_NONE)
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
            int rfp_margin = rfp_depth_mult * depth - rfp_improving_mult * improving;
            if (depth < 10 && eval < VALUE_TB_WIN_IN_MAX_PLY && eval - rfp_margin >= beta)
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
                int R = 4 + depth / 3 + std::min(4, (eval - beta) / nmp_eval_div);

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
            int beta_cut = beta + 174;
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
            if (root_node && !foundRootMove(move))
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
                    if (history_score < -hp_depth_mult * depth && depth < 7)
                        continue;

                    // futility pruning
                    if (!in_check && depth < 11 && eval + fp_base + depth * fp_mult <= alpha)
                        mp.skip_quiets = true;
                }

                // see pruning
                int see_margin = isCap(move) ? -see_cap_margin : -see_quiet_margin;
                int see_depth = isCap(move) ? 6 : 8;
                if (depth <= see_depth && !board.see(move, depth * see_margin))
                    continue;
            }

            if (isCap(move) && c_count < 32)
                c_moves[c_count++] = move;
            else if (q_count < 64)
                q_moves[q_count++] = move;

            // print current move information
            if (id == 0 && root_node && limit.multipv == 1 && tm.elapsedTime() > 5000 && !threads.isStopped())
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
                    // credits to stockfish
                    new_depth += (score > best_score + zws_margin);
                    new_depth -= (score < best_score + new_depth);

                    if (lmr_depth < new_depth)
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
            {
                RootMove *rm = &root_moves[0];
                for (int i = 1; i < root_moves.size(); i++)
                    if (root_moves[i].move == move)
                    {
                        rm = &root_moves[i];
                        break;
                    }

                rm->nodes += nodes - start_nodes;
                rm->avg_score = rm->avg_score == -VALUE_INFINITE ? score : (rm->avg_score + score) / 2;

                if (made_moves == 1 || score > alpha)
                {
                    rm->score = score;
                    rm->seldepth = seldepth;

                    // copy pv line
                    rm->pv[0] = move;

                    rm->pv.length = pv_table[ss->ply + 1].length;
                    for (int i = 1; i < rm->pv.length; i++)
                        rm->pv[i] = pv_table[ss->ply + 1][i];
                }
                else
                    rm->score = -VALUE_INFINITE;
            }

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;

                    if (pv_node && !root_node)
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
        if (!skipped && (!root_node || multipv_idx == 0))
            ent->store(hash, best_move, best_score, raw_eval, bound, depth, ss->ply, tt_pv);

        // update correction histories
        if (!in_check && (!best_move || !isCap(best_move)) && (bound & (best_score >= raw_eval ? LOWER_BOUND : UPPER_BOUND)))
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
            return evaluate();

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
                raw_eval = tt_eval == VALUE_NONE ? evaluate() : tt_eval;
                eval = ss->eval = adjustEval(ss, raw_eval);

                if (tt_score != VALUE_NONE && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else
            {
                raw_eval = evaluate();
                eval = ss->eval = adjustEval(ss, raw_eval);
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

        (ss + 1)->killer = NO_MOVE;

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

    // search eval

    Score Search::evaluate()
    {
        int32_t eval = NNUE::nnue.forward(board.getAccumulator(), board.getTurn());
        // scale based on phase
        eval = (128 + board.getPhase()) * eval / 128;

        return std::clamp(eval, -(VALUE_MATE - MAX_PLY) + 1, VALUE_MATE - MAX_PLY - 1);
    }

    Score Search::adjustEval(const Stack *ss, Score eval) const
    {
        eval += history.getMaterialCorr(board) + history.getContCorr(ss);
        return std::clamp(eval, Score(-VALUE_TB_WIN_IN_MAX_PLY + 1), Score(VALUE_TB_WIN_IN_MAX_PLY - 1));
    }

    // search helper

    bool Search::isLimitReached(int depth) const
    {
        if (threads.isStopped())
            return true;
        if (limit.infinite)
            return false;
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

    void Search::sortRootMoves(int offset)
    {
        assert(offset >= 0);

        for (int i = offset; i < root_moves.size(); i++)
        {
            int best = i;
            for (int j = i + 1; j < root_moves.size(); j++)
                if (root_moves[j].score > root_moves[i].score)
                    best = j;

            if (best != i)
                std::swap(root_moves[i], root_moves[best]);
        }
    }

    bool Search::foundRootMove(const Move &move)
    {
        for (int i = multipv_idx; i < root_moves.size(); i++)
            if (root_moves[i].move == move)
                return true;
        return false;
    }

    void Search::printUciInfo()
    {
        const int64_t elapsed_time = tm.elapsedTime();
        const U64 total_nodes = Astra::threads.getTotalNodes();
        const Score result = root_moves[multipv_idx].score;
        const PVLine &pv_line = root_moves[multipv_idx].pv;

        std::cout << "info depth " << root_depth
                  << " seldepth " << Astra::threads.getSelDepth()
                  << " multipv " << multipv_idx + 1
                  << " score ";

        if (abs(result) >= VALUE_MATE - MAX_PLY)
            std::cout << "mate " << (VALUE_MATE - abs(result) + 1) / 2 * (result > 0 ? 1 : -1);
        else
            std::cout << "cp " << Score(result / 1.8); // normalize

        std::cout << " nodes " << total_nodes
                  << " nps " << total_nodes * 1000 / (elapsed_time + 1)
                  << " tbhits " << Astra::threads.getTotalTbHits()
                  << " hashfull " << Astra::tt.hashfull()
                  << " time " << elapsed_time
                  << " pv";

        // print the pv
        if (!pv_line.length)
            std::cout << " " << pv_line[0];
        else
            for (int i = 0; i < pv_line.length; i++)
                std::cout << " " << pv_line[i];

        std::cout << std::endl;
    }

} // namespace Astra
