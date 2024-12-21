#include <cmath>
#include <algorithm>
#include "search.h"
#include "tune.h"
#include "syzygy.h"
#include "threads.h"
#include "movepicker.h"
#include "../eval/eval.h"

namespace Astra
{
    // search helper

    int REDUCTIONS[MAX_PLY][MAX_MOVES];

    void initReductions()
    {
        REDUCTIONS[0][0] = 0;

        double base = lmr_base / 100.0;
        double div = lmr_div / 100.0;

        for (int depth = 1; depth < MAX_PLY; depth++)
            for (int moves = 1; moves < MAX_MOVES; moves++)
                REDUCTIONS[depth][moves] = base + log(depth) * log(moves) / div;
    }

    // search class

    Search::Search(const std::string &fen) : board(fen)
    {
        tt.clear();
    }

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

        Stack stack[MAX_PLY + 6]; // +6 for history
        Stack *ss = stack + 6;

        for (int i = 0; i < MAX_PLY; ++i)
            (ss + i)->ply = i;

        int avg_eval = 0;
        int move_changes = 0;
        Score previous_result = VALUE_NONE;

        Move best_move = NO_MOVE;
        for (int depth = 1; depth < MAX_PLY; depth++)
        {
            // reset pv
            for (auto &pv_line : pv_table)
                pv_line.length = 0;

            root_depth = depth;
            Score result = aspSearch(depth, previous_result, ss);
            previous_result = result;

            if (isLimitReached(depth))
                break;

            best_move = pv_table[0][0];

            if (id == 0 && depth >= 10 && limit.time.optimum)
            {
                avg_eval += result;
                move_changes += (pv_table[0][0] != best_move);

                // increase time if eval is increasing
                if (result + 30 < avg_eval / depth)
                    limit.time.optimum *= 1.10;
                // increase time if eval is decreasing
                if (result > -200 && result - previous_result < -20)
                    limit.time.optimum *= 1.10;
                // increase optimum time if best move changes often
                if (move_changes > 4)
                    limit.time.optimum = limit.time.max * 0.75;
                // stop search if more than 75% of our max time is reached
                if (tm.elapsedTime() > limit.time.max * 0.75)
                    break;
            }
        }

        if (id == 0)
            std::cout << "bestmove " << best_move << std::endl;

        return best_move;
    }

    Score Search::aspSearch(int depth, Score prev_eval, Stack *ss)
    {
        Score alpha = -VALUE_MATE;
        Score beta = VALUE_MATE;

        int window = asp_window;
        if (depth >= asp_depth)
        {
            alpha = std::max(prev_eval - window, -int(VALUE_MATE));
            beta = std::min(prev_eval + window, int(VALUE_MATE));
        }

        Score result = VALUE_NONE;
        while (true)
        {
            if (alpha < -2000)
                alpha = -VALUE_MATE;

            if (beta > 2000)
                beta = VALUE_MATE;

            result = negamax(depth, alpha, beta, ss, false);

            if (isLimitReached(depth))
                return result;

            if (result <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max(alpha - window, -int(VALUE_MATE));
            }
            else if (result >= beta)
                beta = std::min(beta + window, int(VALUE_MATE));
            else
                break;

            window += window / 3.75;
        }

        if (id == 0)
            printUciInfo(result, depth, pv_table[0]);

        return result;
    }

    Score Search::negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node, const Move skipped)
    {
        assert(alpha < beta);
        assert(ss->ply >= 0);

        if (isLimitReached(depth))
            return 0;

        if (ss->ply >= MAX_PLY - 1)
            return adjustEval(Eval::evaluate(board));

        pv_table[ss->ply].length = 0;

        // variables
        const bool root_node = ss->ply == 0;
        const bool pv_node = beta - alpha != 1;

        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();
        const Score orig_alpha = alpha;
        const U64 hash = board.getHash();

        Score eval = ss->eval;
        bool improving = false;

        Score max_score = VALUE_MATE;
        Score best_score = -VALUE_MATE;

        // dive into quiescence search if depth is less than 1
        if (depth <= 0)
            return qSearch(0, alpha, beta, ss);

        // selective depth
        if (pv_node && ss->ply > sel_depth)
            sel_depth = ss->ply; // heighest depth a pv node has reached

        // check for terminal states
        if (!root_node)
        {
            // mate distance pruning
            alpha = std::max(alpha, Score(ss->ply - VALUE_MATE));
            beta = std::min(beta, Score(VALUE_MATE - ss->ply - 1));
            if (alpha >= beta)
                return alpha;

            if (board.isRepetition(pv_node) || board.isDraw())
                return VALUE_DRAW;
        }

        // look up in transposition table
        TTEntry* ent;
        const bool tt_hit = tt.lookup(ent, hash);
        const Bound tt_bound = ent->getBound();
        const Move tt_move = ent->getMove();
        const Score tt_score = ent->getScore(ss->ply);
        const Score tt_eval = ent->getEval();
        const int tt_depth = ent->getDepth();
        const bool tt_was_pv = ent->wasPv();

        if (!pv_node && !skipped && tt_depth >= depth && tt_score != VALUE_NONE)
            if (tt_bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND))
                return tt_score;

        // reset killer
        (ss + 1)->killer = NO_MOVE;

        // tablebase probing
        if (use_tb && !root_node)
        {
            Score tb_score = probeWDL(board);

            if (tb_score != VALUE_NONE)
            {
                Bound bound = NO_BOUND;
                tb_hits++;

                if (tb_score == VALUE_TB_WIN)
                {
                    tb_score = VALUE_TB_WIN - ss->ply - 1;
                    bound = LOWER_BOUND;
                }
                else if (tb_score == -VALUE_TB_WIN)
                {
                    tb_score = -VALUE_TB_WIN + ss->ply + 1;
                    bound = UPPER_BOUND;
                }
                else
                {
                    tb_score = 0;
                    bound = EXACT_BOUND;
                }

                if (bound == EXACT_BOUND || (bound == LOWER_BOUND && tb_score >= beta) || (bound == UPPER_BOUND && tb_score <= alpha))
                {
                    ent->store(hash, NO_MOVE, tb_score, tt_eval, bound, depth, ss->ply, tt_was_pv);
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
            eval = ss->eval = VALUE_NONE;
        else
        {
            // use tt score for better evaluation
            if (tt_hit)
            {
                eval = tt_eval;
                if (eval == VALUE_NONE)
                    eval = Eval::evaluate(board);

                ss->eval = eval;
                eval = adjustEval(eval);

                if (tt_score != VALUE_NONE && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else if (!skipped)
            {
                eval = ss->eval = Eval::evaluate(board);
                eval = adjustEval(eval);
                ent->store(hash, NO_MOVE, VALUE_NONE, ss->eval, NO_BOUND, -1, ss->ply, tt_was_pv);
            }

            // check for improvement
            if ((ss - 2)->eval != VALUE_NONE)
                improving = ss->eval > (ss - 2)->eval;
            else if ((ss - 4)->eval != VALUE_NONE)
                improving = ss->eval > (ss - 4)->eval;
        }

        /*
        // update quiet history
        Move prev_move = (ss - 1)->curr_move;
        if (!prev_move && !isCap(prev_move) && (ss - 1)->eval != VALUE_NONE) 
        {
            int bonus = std::clamp(-5 * (ss->eval + (ss - 1)->eval), -80, 150);
            history.updateQuietHistory(~stm, prev_move, bonus);
        }*/

        // internal iterative reduction
        if (!in_check && !tt_move && depth >= 4 && (pv_node || cut_node))
            depth--;

        // only use pruning when not in check and pv node
        if (!in_check && !pv_node)
        {
            bool opponent_has_good_captures = board.oppHasGoodCaptures();

            // reverse futility pruning
            int rfp_margin = std::max(rfp_depth_mult * (depth - (improving && !opponent_has_good_captures)), 20);
            if (depth <= 7 && eval < VALUE_TB_WIN_IN_MAX_PLY && eval - rfp_margin >= beta)
                return (eval + beta) / 2;

            // razoring
            if (depth < 5 && eval + rzr_depth_mult * depth < alpha)
            {
                Score score = qSearch(0, alpha, beta, ss);
                if (score <= alpha)
                    return score;
            }

            // null move pruning
            if (depth >= 3 && !skipped && eval >= beta && ss->eval + 30 * depth - 170 >= beta && board.nonPawnMat(stm)
                && !opponent_has_good_captures && (ss - 1)->curr_move != NULL_MOVE && beta > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                int R = 4 + depth / nmp_depth_div + std::min(int(nmp_min), (eval - beta) / nmp_div);

                ss->curr_move = NULL_MOVE;

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
            int beta_cut = beta + probcut_margin;
            if (depth > 4 && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY && !(tt_depth >= depth - 3 && tt_hit && tt_score < beta_cut))
            {
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

                    board.makeMove(move, true);
                    Score score = -qSearch(0, -beta_cut, -beta_cut + 1, ss + 1);

                    if (score >= beta_cut)
                        score = -negamax(depth - 4, -beta_cut, -beta_cut + 1, ss + 1, !cut_node);

                    board.unmakeMove(move);

                    if (score >= beta_cut)
                    {
                        ent->store(hash, move, score, ss->eval, LOWER_BOUND, depth - 3, ss->ply, tt_was_pv);
                        return score;
                    }
                }
            }
        }

        MovePicker mp(N_SEARCH, board, history, ss, tt_move);

        int made_moves = 0, q_count = 0, c_count = 0;

        Move q_moves[64];
        Move c_moves[32];
        Move best_move = NO_MOVE, move;

        while ((move = mp.nextMove()) != NO_MOVE)
        {
            if (move == skipped || !board.isLegal(move))
                continue;

            // prefetch tt entry
            tt.prefetch(board.keyAfter(move));

            made_moves++;

            int history_score = isCap(move) ? history.getCapHistory(board, move) : history.getQuietHistory(board, ss, move);

            // reductions
            int r = REDUCTIONS[depth][made_moves];
            // decrease/increase based on history score
            r -= history_score / hp_div;
            // increase when not improving
            r += !improving;

            if (!root_node && best_score > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                int lmr_depth = std::max(1, depth - r);

                // late move pruning
                if (q_count > (3 + depth * depth) / (2 - improving))
                    mp.skip_quiets = true;

                if (!isCap(move) && move.type() != PQ_QUEEN)
                {
                    // history pruning
                    if (history_score < -hp_margin * depth && lmr_depth < 5)
                        continue;

                    // futility pruning
                    if (!in_check && lmr_depth <= 9 && eval + fp_base + lmr_depth * fp_mult <= alpha)
                        mp.skip_quiets = true;
                }

                // see pruning
                int see_margin = isCap(move) ? depth * see_cap_margin : lmr_depth * see_quiet_margin;
                if (!board.see(move, -see_margin))
                    continue;
            }

            if (isCap(move) && c_count < 32)
                c_moves[c_count++] = move;
            else if (q_count < 64)
                q_moves[q_count++] = move;

            // print current move information
            if (id == 0 && root_node && tm.elapsedTime() > 5000 && !threads.isStopped())
            {
                std::cout << "info depth " << depth
                          << " currmove " << move
                          << " currmovenumber " << made_moves << std::endl;
            }

            int extension = 0;

            // singular extensions
            if (!root_node && depth >= 6 && ss->ply < 2 * root_depth && !skipped && tt_move == move 
                && tt_depth >= depth - 3 && (tt_bound & LOWER_BOUND) && std::abs(tt_score) < VALUE_TB_WIN_IN_MAX_PLY)
            {
                Score sbeta = tt_score - 3 * depth;
                int sdepth = (depth - 1) / 2;

                Score score = negamax(sdepth, sbeta - 1, sbeta, ss, cut_node, move);

                if (score < sbeta)
                {
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
            }

            int new_depth = depth - 1 + extension;

            nodes++;
            ss->curr_move = move;
            board.makeMove(move, true);

            Score score = VALUE_NONE;

            // late move reductions
            if (depth > 2 && made_moves > 1 && (!pv_node || !isCap(move)))
            {
                // increase when tt move is a capture or promotion
                r += isCap(tt_move) || isProm(tt_move);
                // increase when in a cut node
                r += 2 * cut_node;
                // decrease when in pv node
                r -= (pv_node + tt_was_pv);
                // decrease when move gives check
                r -= board.inCheck();
                // decrease when tt depth is greater or equal to current depth 
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

                    int bonus = score <= alpha ? -historyBonus(new_depth) : score >= beta ? historyBonus(new_depth) : 0;
                    history.updateContH(board, move, ss, bonus);
                }
            }
            else if (!pv_node || made_moves > 1)
                // full-depth search if lmr was skipped
                score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1, !cut_node);

            // principal variation search
            if (pv_node && ((score > alpha && (root_node || score < beta)) || made_moves == 1))
                score = -negamax(new_depth, -beta, -alpha, ss + 1, false);

            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;

                    // update pv node when in pv node
                    if (id == 0 && pv_node)
                    {
                        pv_table[ss->ply][0] = move;
                        for (int i = 0; i < pv_table[ss->ply + 1].length; i++)
                            pv_table[ss->ply][i + 1] = pv_table[ss->ply + 1][i];
                        pv_table[ss->ply].length = pv_table[ss->ply + 1].length + 1;
                    }
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
            return skipped != NO_MOVE ? alpha : in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;

        best_score = std::min(best_score, max_score);

        // store in transposition table
        if (!skipped)
        {
            Bound bound = best_score >= beta ? LOWER_BOUND : best_score <= orig_alpha ? UPPER_BOUND : EXACT_BOUND;
            ent->store(hash, best_move, best_score, ss->eval, bound, depth, ss->ply, tt_was_pv);
        }

        return best_score;
    }

    Score Search::qSearch(int depth, Score alpha, Score beta, Stack *ss)
    {
        assert(alpha < beta);

        if (isLimitReached(1))
            return 0;

        const bool pv_node = beta - alpha != 1;
        const bool in_check = board.inCheck();

        if (board.isRepetition(pv_node) || board.isDraw())
            return VALUE_DRAW;

        if (ss->ply >= MAX_PLY - 1)
            return adjustEval(Eval::evaluate(board));

        // variables
        const U64 hash = board.getHash();
        Score best_score = -VALUE_MATE + ss->ply;
        Score eval = ss->eval;
        Score futility = -VALUE_MATE;
        Move best_move = NO_MOVE;

        // look up in transposition table
        TTEntry* ent;
        const bool tt_hit = tt.lookup(ent, hash);
        const Move tt_move = ent->getMove();
        const Bound tt_bound = ent->getBound();
        const Score tt_score = ent->getScore(ss->ply);
        const Score tt_eval = ent->getEval();
        const bool tt_was_pv = ent->wasPv();

        if (!pv_node && tt_score != VALUE_NONE && (tt_bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND)))
            return tt_score;

        // set eval and static eval
        if (in_check)
            eval = ss->eval = VALUE_NONE;
        else
        {
            // use tt score for better evaluation
            if (tt_hit)
            {
                eval = tt_eval;
                if (eval == VALUE_NONE)
                    eval = Eval::evaluate(board);

                ss->eval = eval;
                eval = adjustEval(eval);

                if (tt_score != VALUE_NONE && (tt_bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else
            {
                eval = ss->eval = Eval::evaluate(board);
                eval = adjustEval(eval);
                ent->store(hash, NO_MOVE, VALUE_NONE, ss->eval, NO_BOUND, -1, ss->ply, tt_was_pv);
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
        ent->store(hash, best_move, best_score, ss->eval, bound, 0, ss->ply, tt_was_pv);

        return best_score;
    }

    int Search::adjustEval(Score eval) const
    {
        return (200 - board.halfMoveClock()) * eval / 200;
    }

    bool Search::isLimitReached(const int depth) const
    {
        if (limit.infinite)
            return false;
        if (threads.isStopped())
            return true;
        if (limit.nodes != 0 && nodes >= limit.nodes)
            return true;
        if (depth > limit.depth)
            return true;

        int elapsed_time = tm.elapsedTime();
        if (limit.time.optimum != 0 && elapsed_time >= limit.time.optimum)
            return true;
        if (limit.time.max != 0 && elapsed_time >= limit.time.max)
            return true;

        return false;
    }

    void Search::printUciInfo(Score result, int depth, PVLine &pv_line) const
    {
        std::cout << "info depth " << depth
                  << " seldepth " << threads.getSelDepth()
                  << " score ";

        if (abs(result) >= VALUE_MATE - MAX_PLY)
            std::cout << "mate " << (VALUE_MATE - abs(result) + 1) / 2 * (result > 0 ? 1 : -1);
        else
            std::cout << "cp " << result;

        U64 elapsed_time = tm.elapsedTime();
        U64 total_nodes = threads.getTotalNodes();

        std::cout << " nodes " << total_nodes
                  << " nps " << total_nodes * 1000 / (elapsed_time + 1)
                  << " tbhits " << threads.getTotalTbHits()
                  << " hashfull " << tt.hashfull()
                  << " time " << elapsed_time
                  << " pv";

        // print the pv
        for (int i = 0; i < pv_line.length; i++)
            std::cout << " " << pv_line[i];

        std::cout << std::endl;
    }

} // namespace Astra
