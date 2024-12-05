#include <cmath>
#include <algorithm>
#include "search.h"
#include "syzygy.h"
#include "threads.h"
#include "movepicker.h"
#include "tune.h"
#include "../eval/eval.h"

namespace Astra
{
    // search parameters
    PARAM(lmr_base, 109, 80, 120, 10);
    PARAM(lmr_div, 185, 150, 200, 10);

    PARAM(asp_depth, 8, 5, 9, 1);
    PARAM(asp_window, 11, 5, 30, 5);

    PARAM(rzr_depth, 4, 3, 5, 1);
    PARAM(rzr_depth_mult, 174, 150, 250, 15);

    PARAM(rfp_depth, 11, 9, 11, 1);
    PARAM(rfp_depth_mult, 78, 60, 110, 12);

    PARAM(nmp_min, 4, 3, 6, 1);
    PARAM(nmp_depth_div, 5, 3, 15, 1);
    PARAM(nmp_div, 211, 190, 235, 10);

    PARAM(probcut_margin, 165, 130, 180, 20);

    PARAM(see_cap_margin, 96, 70, 120, 10);
    PARAM(see_quiet_margin, 97, 70, 120, 10);

    PARAM(fp_depth, 10, 9, 11, 1);
    PARAM(fp_base, 169, 120, 180, 15);
    PARAM(fp_mult, 128, 70, 150, 10);

    PARAM(zws_margin, 74, 60, 90, 8);

    PARAM(hp_margin, 4531, 2500, 5000, 400);
    PARAM(hp_div, 6803, 5000, 8500, 400);
    PARAM(hbonus_margin, 66, 65, 80, 5);

    PARAM(qfp_margin, 96, 60, 150, 15);

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

    Move Search::start()
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

        Stack stack[MAX_PLY + 6];
        Stack *ss = stack + 6; // +6 to avoid stack underflow (history accesses ss - 2)

        for (int i = 6; i > 0; --i)
            (ss - i)->ply = i;
        for (int i = 0; i < MAX_PLY; ++i)
            (ss + i)->ply = i;

        int avg_eval = 0;
        int move_changes = 0;
        Score previous_result = VALUE_NONE;

        Move best_move = NO_MOVE;
        for (int depth = 1; depth < MAX_PLY; depth++)
        {
            Score result = aspSearch(depth, previous_result, ss);
            previous_result = result;

            if (isLimitReached(depth))
                break;

            avg_eval += result;
            best_move = pv_table[0][0];

            if (id == 0 && depth >= 5 && limit.time.optimum)
            {
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
                if (depth > 10 && tm.elapsedTime() > limit.time.max * 0.75) 
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

        // only use aspiration window when depth is higher or equal to 6
        int window = asp_window;
        if (depth >= asp_depth)
        {
            alpha = prev_eval - window;
            beta = prev_eval + window;
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

    Score Search::negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node)
    {
        assert(alpha < beta);
        assert(ss->ply >= 0);

        if (isLimitReached(depth))
            return beta;

        if (ss->ply >= MAX_PLY - 1)
            return Eval::evaluate(board);

        pv_table[ss->ply].length = ss->ply;

        // variables
        const bool root_node = ss->ply == 0;
        const bool pv_node = beta - alpha != 1;

        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();
        const int orig_alpha = alpha;

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
            Score mating_value = VALUE_MATE - ss->ply;
            if (mating_value < beta)
            {
                beta = mating_value;
                if (alpha >= mating_value)
                    return mating_value; // beta cut-off
            }

            mating_value = -VALUE_MATE + ss->ply;
            if (mating_value > alpha)
            {
                alpha = mating_value;
                if (beta <= mating_value)
                    return mating_value; // alpha cut-off
            }

            if (board.isRepetition(pv_node) || board.isDraw())
                return VALUE_DRAW;
        }

        // variables
        int eval = ss->static_eval;
        bool improving = false;

        Score max_score = VALUE_MATE;
        Score best_score = -VALUE_MATE;

        // look up in transposition table
        TTEntry ent;
        U64 hash = board.getHash();
        bool tt_hit = tt.lookup(ent, hash);
        const Score tt_score = tt_hit ? scoreFromTT(ent.score, ss->ply) : Score(VALUE_NONE);

        if (!pv_node && !ss->skipped && tt_hit && ent.depth >= depth && tt_score != VALUE_NONE)
            if (ent.bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND))
                return tt_score;

        // reset some variables
        (ss + 1)->skipped = NO_MOVE;
        (ss + 1)->killer1 = NO_MOVE;
        (ss + 1)->killer2 = NO_MOVE;

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
                    tt.store(hash, NO_MOVE, scoreToTT(tb_score, ss->ply), depth + 6, bound);
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
            eval = ss->static_eval = VALUE_NONE;
        else
        {
            // use tt score for better evaluation
            if (tt_hit)
            {
                eval = tt_score;
                if (eval == VALUE_NONE)
                    eval = Eval::evaluate(board);

                ss->static_eval = eval;

                if (tt_score != VALUE_NONE && (ent.bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else if (!ss->skipped)
            {
                eval = ss->static_eval = Eval::evaluate(board);
                tt.store(hash, NO_MOVE, scoreToTT(eval, ss->ply), -1, NO_BOUND);
            }

            // check for improvement
            if ((ss - 2)->static_eval != VALUE_NONE)
                improving = ss->static_eval > (ss - 2)->static_eval;
            else if ((ss - 4)->static_eval != VALUE_NONE)
                improving = ss->static_eval > (ss - 4)->static_eval;
        }

        // internal iterative reduction
        if (!in_check && !tt_hit && depth >= 4 && (pv_node || cut_node))
            depth--;

        // only use pruning when not in check and pv node
        if (!in_check && !pv_node)
        {
            // reverse futility pruning
            int rfp_margin = std::max(rfp_depth_mult * (depth - improving), 20);
            if (depth <= rfp_depth && eval < VALUE_TB_WIN_IN_MAX_PLY && eval - rfp_margin >= beta)
                return (eval + beta) / 2;

            // razoring
            if (depth <= rzr_depth && eval + rzr_depth_mult * depth < alpha)
            {
                Score score = qSearch(0, alpha, beta, ss);
                if (score < alpha)
                    return score;
            }

            // null move pruning
            if (depth >= 4 && !ss->skipped && eval >= beta && board.nonPawnMat(stm) && (ss - 1)->curr_move != NULL_MOVE)
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
            if (depth > 3 && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY 
                && !(ent.depth >= depth - 3 && tt_score != VALUE_NONE && tt_score < beta_cut))
            {
                MovePicker movepicker(PC_SEARCH, board, history, ss, ent.move);

                Move move = NO_MOVE;
                while ((move = movepicker.nextMove()) != NO_MOVE)
                {
                    nodes++;
                    ss->curr_move = move;
                    board.makeMove(move, true);

                    Score score = -qSearch(0, -beta_cut, -beta_cut + 1, ss + 1);

                    if (score >= beta_cut)
                        score = -negamax(depth - 4, -beta_cut, -beta_cut + 1, ss + 1, !cut_node);

                    board.unmakeMove(move);

                    if (score >= beta_cut)
                    {
                        tt.store(hash, move, scoreToTT(score, ss->ply), depth - 3, LOWER_BOUND);
                        return score;
                    }
                }
            }
        }

        MovePicker mp(N_SEARCH, board, history, ss, ent.move);

        bool is_ttmove_cap = board.isCapture(ent.move);

        int made_moves = 0, q_count = 0, c_count = 0;

        Move q_moves[64];
        Move c_moves[32];
        Move best_move = NO_MOVE, move = NO_MOVE;

        while ((move = mp.nextMove()) != NO_MOVE)
        {
            if (move == ss->skipped)
                continue;

            made_moves++;

            bool is_cap = board.isCapture(move);

            int history_score;
            if (is_cap)
                history_score = history.getCHScore(board, move);
            else
                history_score = history.getQHScore(stm, move);

            int r = REDUCTIONS[depth][made_moves];
            // decrease/increase based on history score
            r -= history_score / hp_div;

            if (!root_node && board.nonPawnMat(stm) && best_score > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                int lmr_depth = std::max(1, depth - r);
                
                if (!is_cap && !isPromotion(move))
                {
                    // late move pruning
                    if (q_count > (3 + depth * depth) / (2 - improving))
                        continue;

                    // history pruning
                    if (history_score < -hp_margin * depth && lmr_depth < 5) 
                        continue;

                    // futility pruning
                    if (!in_check && lmr_depth <= fp_depth && eval + fp_base + lmr_depth * fp_mult <= alpha) 
                        continue;
                }
            
                // see pruning
                if (!board.see(move, -(is_cap ? depth * see_cap_margin : lmr_depth * see_quiet_margin)))
                    continue;   
            }

            if (is_cap && c_count < 32)
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
            if (!root_node && tt_hit && depth >= 6 
                && !ss->skipped && ent.move == move && ent.depth >= depth - 3 
                && (ent.bound & LOWER_BOUND) && std::abs(tt_score) < VALUE_TB_WIN_IN_MAX_PLY)
            {
                Score sbeta = tt_score - 3 * depth;
                int sdepth = (depth - 1) / 2;

                ss->skipped = move;
                Score score = negamax(sdepth, sbeta - 1, sbeta, ss, cut_node);
                ss->skipped = NO_MOVE;

                if (score < sbeta) 
                {
                    if (!pv_node && score < sbeta - 14)
                        extension = 2;
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

            // late move reduction
            if (depth > 1 && made_moves > 1 && !(pv_node && in_check))
            {
                // increase when tt move is a capture
                r += is_ttmove_cap;
                // increase when not improving
                r += !improving;
                // increase when in a cut node
                r += 2 * cut_node;
                // decrease when in pv node
                r -= pv_node;
                // decrease when move gives check
                r -= board.inCheck();

                int lmr_depth = std::clamp(new_depth - r, 1, new_depth + 1);

                score = -negamax(lmr_depth, -alpha - 1, -alpha, ss + 1, true);

                // if late move reduction failed high and we actually reduced, do a research
                if (score > alpha && lmr_depth < new_depth)
                {
                    new_depth += (score > best_score + zws_margin);
                    new_depth -= (score < best_score + new_depth);

                    if (new_depth > lmr_depth)
                        score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1, !cut_node);

                    int bonus = score <= alpha ? -historyBonus(new_depth) : score >= beta ? historyBonus(new_depth) : 0;
                    history.updateContH(board, move, ss, bonus);
                }
            }
            else if (!pv_node || made_moves > 1)
                // full-depth search if lmr was skipped
                score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1, !cut_node);

            // principal variation search
            if (pv_node && (score > alpha || made_moves == 1))
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
                    // update best move when in pv node
                    if (pv_node)
                        updatePV(ss->ply, best_move);
                }

                if (score >= beta)
                {
                    history.update(board, move, ss, q_moves, q_count, c_moves, c_count, depth + (best_score > beta + hbonus_margin));
                    // cut-off
                    break;
                }
            }
        }

        // check for mate and stalemate
        if (mp.getMoveCount() == 0)
            return ss->skipped != NO_MOVE ? alpha : in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;

        best_score = std::min(best_score, max_score);

        // store in transposition table
        if (!ss->skipped)
        {
            Bound bound = best_score >= beta ? LOWER_BOUND : best_score <= orig_alpha ? UPPER_BOUND : EXACT_BOUND;
            tt.store(hash, best_move, scoreToTT(best_score, ss->ply), depth, bound);
        }

        return best_score;
    }

    Score Search::qSearch(int depth, Score alpha, Score beta, Stack *ss)
    {
        assert(alpha < beta);

        if (isLimitReached(1))
            return beta;

        const bool pv_node = beta - alpha != 1;
        const bool in_check = board.inCheck();

        if (board.isRepetition(pv_node) || board.isDraw())
            return VALUE_DRAW;

        if (ss->ply >= MAX_PLY - 1)
            return Eval::evaluate(board);

        Score best_score = -VALUE_MATE + ss->ply;
        int eval = ss->static_eval;
        int futility = -VALUE_MATE;

        // look up in transposition table
        TTEntry ent;
        U64 hash = board.getHash();
        bool tt_hit = tt.lookup(ent, hash);
        const Score tt_score = tt_hit ? scoreFromTT(ent.score, ss->ply) : Score(VALUE_NONE);

        if (!pv_node && tt_hit && tt_score != VALUE_NONE && (ent.bound & (tt_score >= beta ? LOWER_BOUND : UPPER_BOUND)))
            return tt_score;

        // set eval and static eval
        if (in_check)
            eval = ss->static_eval = VALUE_NONE;
        else
        {
            // use tt score for better evaluation
            if (tt_hit)
            {
                eval = tt_score;
                if (eval == VALUE_NONE)
                    eval = Eval::evaluate(board);

                ss->static_eval = eval;

                if (tt_score != VALUE_NONE && (ent.bound & (tt_score > eval ? LOWER_BOUND : UPPER_BOUND)))
                    eval = tt_score;
            }
            else
            {
                eval = ss->static_eval = Eval::evaluate(board);
                tt.store(hash, NO_MOVE, scoreToTT(eval, ss->ply), -1, NO_BOUND);
            }

            best_score = eval;
            futility = eval + qfp_margin;

            // stand pat
            if (best_score >= beta)
                return best_score;
            if (best_score > alpha)
                alpha = best_score;
        }

        MovePicker mp(Q_SEARCH, board, history, ss, ent.move, in_check, depth >= -1);

        Move best_move = NO_MOVE;
        Move move = NO_MOVE;

        while ((move = mp.nextMove()) != NO_MOVE)
        {
            bool is_cap = board.isCapture(move);

            if (best_score > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                if (!in_check && futility <= alpha && is_cap && !board.see(move, 1))
                {
                    best_score = std::max(best_score, Score(futility));
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

                if (score >= beta)
                    break; // cut-off
            }

            if (best_score > -VALUE_TB_WIN_IN_MAX_PLY && in_check && !is_cap && mp.ml_tt_move != move) 
                break;
        }

        if (mp.getMoveCount() == 0 && in_check)
            return -VALUE_MATE + ss->ply;

        if (best_score >= beta && abs(best_score) < VALUE_TB_WIN_IN_MAX_PLY)
            best_score = (best_score + beta) / 2;

        // store in transposition table
        Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND; // no exact bound in qsearch
        tt.store(hash, best_move, scoreToTT(best_score, ss->ply), 0, bound);

        return best_score;
    }

    void Search::updatePV(int ply, Move& m)
    {
        PVLine &current_pv = pv_table[ply];
        current_pv[ply] = m;

        uint8_t next_length = pv_table[ply + 1].length;
        for (int next_ply = ply + 1; next_ply < next_length; next_ply++)
            current_pv[next_ply] = pv_table[ply + 1][next_ply];

        current_pv.length = next_length;
    }

    bool Search::isLimitReached(const int depth) const
    {
        if (limit.infinite)
            return false;

        if (threads.isStopped())
            return true;

        if (limit.time.optimum != 0 && tm.elapsedTime() > limit.time.optimum)
            return true;

        if (limit.nodes != 0 && nodes >= limit.nodes)
            return true;

        if (depth > limit.depth)
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

    // global variable
    TTable tt(16);

} // namespace Astra
