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
    PARAM(lmr_base, 100, 80, 130, 8);
    PARAM(lmr_div, 180, 150, 200, 8);

    PARAM(asp_depth, 8, 6, 9, 1);
    PARAM(asp_window, 8, 5, 20, 3);

    PARAM(rzr_depth, 3, 3, 5, 1);
    PARAM(rzr_depth_mult, 159, 150, 250, 15);
    
    PARAM(rfp_depth, 10, 7, 11, 1);
    PARAM(rfp_depth_mult, 75, 50, 100, 5);

    PARAM(nmp_base, 5, 4, 5, 1);
    PARAM(nmp_min, 4, 3, 6, 1);
    PARAM(nmp_depth_div, 6, 3, 6, 1);
    PARAM(nmp_div, 214, 200, 220, 2);

    PARAM(probcut_margin, 142, 130, 180, 8);

    PARAM(see_cap_margin, 100, 85, 110, 3);
    PARAM(see_quiet_margin, 90, 75, 100, 3);

    PARAM(fp_depth, 10, 7, 11, 1);
    PARAM(fp_base, 146, 120, 180, 10);
    PARAM(fp_mult, 101, 85, 110, 5);

    PARAM(hp_margin, 3780, 2500, 5000, 400);
    PARAM(hp_div, 5962, 5000, 8500, 400);
    PARAM(hp_depth, 5, 4, 6, 1);
    PARAM(history_bonus_margin, 76, 55, 100, 5);

    PARAM(qfp_margin, 110, 60, 150, 10);

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
        for (auto& pvLine : pv_table)
            pvLine.length = 0;

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

        for(int i = 6; i > 0 ; --i)
            (ss - i)->ply = i;
        for (int i = 0; i < MAX_PLY; ++i)
            (ss + i)->ply = i;

        int avg_eval = 0;

        int best_move_changes = 0;
        Score previous_result = VALUE_NONE;

        Move best_move = NO_MOVE;
        for (int depth = 1; depth < MAX_PLY; depth++)
        {
            Score result = aspSearch(depth, previous_result, ss);
            previous_result = result;

            if (isLimitReached(depth))
                break;

            best_move_changes = best_move != pv_table[0][0]; 
            avg_eval += result;

            best_move = pv_table[0][0];

            if (id == 0) 
            {
                printUciInfo(result, depth, pv_table[0]); 

                // skip time managament if no time is set
                if (!limit.time.optimum || !limit.time.max)
                    continue;

                // increase time if eval is increasing
                if (result + 30 < avg_eval / depth) 
                    limit.time.optimum *= 1.10;

                // increase time if eval is decreasing
                if (result > -200 && result - previous_result < -20) 
                    limit.time.optimum *= 1.10;

                // increase optimum time if best move changes often
                if (best_move_changes > 4) 
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
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;

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
            if (alpha < -1500)
                alpha = -VALUE_INFINITE;
            
            if (beta > 1500) 
                beta = VALUE_INFINITE;
            
            result = negamax(depth, alpha, beta, ss);

            if (isLimitReached(depth))
                return result;

            if (result <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max(alpha - window, -int(VALUE_INFINITE));
            }
            else if (result >= beta)
                beta = std::min(beta + window, int(VALUE_INFINITE));
            else
                break;

            window += window / 3;
        }

        return result;
    }

    Score Search::negamax(int depth, Score alpha, Score beta, Stack *ss)
    {
        assert(alpha < beta);
        assert(ss->ply >= 0);

        if (isLimitReached(depth))
            return beta;

        pv_table[ss->ply].length = ss->ply;

        // variables
        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();

        const bool root_node = ss->ply == 0;
        const bool pv_node = beta - alpha != 1;

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

            if (ss->ply >= MAX_PLY - 1)
                return in_check ? 0 : Eval::evaluate(board);

            if (board.isRepetition(pv_node) || board.isDraw())
                return VALUE_DRAW;
        }

        // dive into quiescence search if depth is less than 1
        if (depth <= 0)
            return qSearch(0, alpha, beta, ss);

        (ss + 1)->skipped = NO_MOVE;

        // selective depth
        if (pv_node && ss->ply > sel_depth)
            sel_depth = ss->ply; // heighest depth a pv node has reached

        // variables
        int eval = ss->static_eval;
        bool improving = false;

        Score max_score = VALUE_MATE;
        Score best_score = -VALUE_MATE;

        // look up in transposition table
        TTEntry ent;
        U64 hash = board.getHash();
        bool tt_hit = tt.lookup(ent, hash);
        Score tt_score = tt_hit ? scoreFromTT(ent.score, ss->ply) : Score(VALUE_NONE);

        if (!pv_node && tt_hit && !ss->skipped && ent.depth >= depth && tt_score != VALUE_NONE)
        {
            if (ent.bound == EXACT_BOUND)
                return tt_score;
            else if (ent.bound == LOWER_BOUND && tt_score >= beta)
                return tt_score;
            else if (ent.bound == UPPER_BOUND && tt_score <= alpha)
                return tt_score;
        }

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

                if (bound == EXACT_BOUND ||
                    (bound == LOWER_BOUND && tb_score >= beta) ||
                    (bound == UPPER_BOUND && tb_score <= alpha))
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

                if (tt_score != VALUE_NONE) 
                {
                    if (ent.bound == EXACT_BOUND)
                        eval = tt_score;
                    else if (ent.bound == LOWER_BOUND && eval < tt_score)
                        eval = tt_score;
                    else if (ent.bound == UPPER_BOUND && eval > tt_score)
                        eval = tt_score;
                }
            } else if (!ss->skipped) {
                eval = ss->static_eval = Eval::evaluate(board);
                tt.store(hash, NO_MOVE, scoreToTT(eval, ss->ply), -1, NO_BOUND);
            }

            // check for improvement
            if (ss->ply >= 2)
            {
                if (ss->ply >= 4 && (ss - 2)->static_eval == VALUE_NONE)
                    // if ss - 2 eval was not calculated, use ss - 4 eval
                    improving = ss->static_eval > (ss - 4)->static_eval || (ss - 4)->static_eval == VALUE_NONE;
                else
                    improving = ss->static_eval > (ss - 2)->static_eval || (ss - 2)->static_eval == VALUE_NONE;
            }
        }
   
        // internal iterative reduction
        if (!in_check && !ss->skipped && !tt_hit && depth >= 4 && pv_node)
            depth--;

        // only use pruning when not in check and pv node
        if (!in_check && !pv_node)
        {
            // reverse futility pruning
            int rfp_margin = rfp_depth_mult * (depth - improving);
            if (!ss->skipped && depth <= rfp_depth && eval < VALUE_TB_WIN_IN_MAX_PLY && eval - rfp_margin >= beta)
                return (eval + beta) / 2;

            // razoring
            if (depth <= rzr_depth && eval + rzr_depth_mult * depth <= alpha)
            {
                Score score = qSearch(0, alpha, beta, ss);
                if (score <= alpha)
                    return score;
            }

            // null move pruning
            if (depth >= 4 && !ss->skipped && eval >= beta && board.nonPawnMat(stm) && (ss - 1)->curr_move != NULL_MOVE)
            {
                int R = nmp_base + depth / nmp_depth_div + std::min(int(nmp_min), (eval - beta) / nmp_div);

                ss->curr_move = NULL_MOVE;

                board.makeNullMove();
                Score score = -negamax(depth - R, -beta, -beta + 1, ss + 1);
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
            if (depth > 3
                && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY 
                && !(ent.depth >= depth - 3 && tt_score != VALUE_NONE && tt_score < beta_cut))
            {
                MovePicker movepicker(PC_SEARCH, board, history, ss, ent.move);

                Move move = NO_MOVE;
                while ((move = movepicker.nextMove(true)) != NO_MOVE)
                {
                    nodes++;
                    ss->curr_move = move;
                    board.makeMove(move, true);

                    Score score = -qSearch(0, -beta_cut, -beta_cut + 1, ss + 1);

                    if (score >= beta_cut)
                        score = -negamax(depth - 4, -beta_cut, -beta_cut + 1, ss + 1);

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

        bool skip_quiets = false;
        bool is_ttmove_cap = board.isCapture(ent.move);

        int made_moves = 0, q_count = 0, c_count = 0;

        Move q_moves[64];
        Move c_moves[64];
        Move best_move = NO_MOVE, move = NO_MOVE;
        
        while ((move = mp.nextMove(skip_quiets)) != NO_MOVE)
        {
            if (move == ss->skipped)
                continue;

            made_moves++;

            bool is_cap = board.isCapture(move);

            int history_score = 0;
            if (is_cap) 
                history_score = history.getCHScore(board, move);
            else 
                history_score = history.getQHScore(stm, move);
            
            if (!root_node && best_score > -VALUE_TB_WIN_IN_MAX_PLY) 
            {
                int r = REDUCTIONS[depth][made_moves] + !improving - history_score / hp_div;
                int lmr_depth = std::max(1, depth - r);
                
                // see pruning
                int see_margin = is_cap ? depth * see_cap_margin : lmr_depth * see_quiet_margin;
                if (!board.see(move, -see_margin))
                    continue;

                // late move pruning
                if (q_count > (4 + depth * depth)) 
                    skip_quiets = true;

                if (!is_cap && !isPromotion(move)) 
                {
                    // history pruning
                    if (history_score < -hp_margin * depth && lmr_depth < hp_depth) 
                        skip_quiets = true;
                    
                    // futility pruning
                    if (!in_check && lmr_depth <= fp_depth && eval + fp_base + lmr_depth * fp_mult <= alpha) 
                        skip_quiets = true;
                }
            }

            if (is_cap)
                c_moves[c_count++] = move;
            else 
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
            if (!root_node 
                && tt_hit 
                && depth >= 6 
                && !ss->skipped 
                && ent.move == move 
                && ent.depth >= depth - 3
                && ent.bound & LOWER_BOUND 
                && std::abs(tt_score) < VALUE_TB_WIN_IN_MAX_PLY)
            {
                Score sbeta = tt_score - 3 * depth;
                int sdepth = (depth - 1) / 2;

                ss->skipped = move;
                Score score = negamax(sdepth, sbeta - 1, sbeta, ss);
                ss->skipped = NO_MOVE;

                if (score < sbeta) 
                     extension = 1;
                else if (sbeta >= beta)
                    return sbeta;
                else if (tt_score >= beta)
                    extension = -2 + pv_node;
                else if (tt_score <= alpha)
                    extension = -1;
            }

            const int new_depth = depth - 1 + extension;
          
            nodes++;
            ss->curr_move = move;
            board.makeMove(move, true);
           
            Score score = VALUE_NONE;

            // late move reduction
            if (depth > 1 && made_moves > 1 + pv_node * 2 && (!pv_node || !is_cap))
            {
                int r = REDUCTIONS[depth][made_moves + 1];
                // increase when tt move is capture
                r += is_ttmove_cap;
                // increase when not improving
                r += !improving;
                // increase when not in pv
                r += !pv_node;
                // decrease when move gives check
                r -= board.inCheck();
                // decrease in high history scores
                r -= history_score / hp_div;
                // decrease when move is a killer
                r -= (move == mp.killer1 || move == mp.killer2);

                int lmr_depth = std::clamp(new_depth - r, 1, new_depth + 1);

                score = -negamax(lmr_depth, -alpha - 1, -alpha, ss + 1);

                // if late move reduction failed high and we actually reduced, do a research
                if (score > alpha && r > 1) 
                {
                    if (lmr_depth < new_depth)
                        score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1);

                    int bonus = (score <= alpha ? -1 : score >= beta ? 1 : 0) * historyBonus(new_depth);
                    history.updateContH(board, move, ss, bonus);
                }
            }
            else if (!pv_node || made_moves > 1)
                // full-depth search if lmr was skipped
                score = -negamax(new_depth, -alpha - 1, -alpha, ss + 1);

            // principal variation search
            if (pv_node && ((score > alpha && score < beta) || made_moves == 1))
                score = -negamax(new_depth, -beta, -alpha, ss + 1);

            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;
                    
                    // update best move
                    if(pv_node)
                        updatePV(ss->ply, best_move);
                }

                if (score >= beta)
                {
                    history.update(board, move, ss,q_moves, q_count, c_moves, c_count, depth + (best_score > beta + history_bonus_margin));
                    // cut-off
                    break;
                }
            }
        }

        // check for mate and stalemate
        if (mp.getMoveCount() == 0)
        {
            if (ss->skipped != NO_MOVE)
                return alpha;
            else
                return in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;
        }

        if (pv_node)
            best_score = std::min(best_score, max_score);

        // store in transposition table
        if (!ss->skipped)
        {
            Bound bound;
            if (best_score >= beta)
                bound = LOWER_BOUND;
            else if (pv_node && best_move != NO_MOVE)
                bound = EXACT_BOUND;
            else
                bound = UPPER_BOUND;

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
            return in_check ? 0 : Eval::evaluate(board);

        Score best_score = -VALUE_MATE + ss->ply;
        int eval = ss->static_eval;
        int futility = -VALUE_MATE;

        // look up in transposition table
        TTEntry ent;
        const U64 hash = board.getHash();
        const bool tt_hit = tt.lookup(ent, hash);
        const Score tt_score = tt_hit ? scoreFromTT(ent.score, ss->ply) : Score(VALUE_NONE);

        if (!pv_node && tt_hit && tt_score != VALUE_NONE)
        {
            if (ent.bound == EXACT_BOUND)
                return tt_score;
            else if (ent.bound == LOWER_BOUND && tt_score >= beta)
                return tt_score;
            else if (ent.bound == UPPER_BOUND && tt_score <= alpha)
                return tt_score;
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

                if (tt_score != VALUE_NONE) 
                {
                    if (ent.bound == EXACT_BOUND)
                        eval = tt_score;
                    else if (ent.bound == LOWER_BOUND && eval < tt_score)
                        eval = tt_score;
                    else if (ent.bound == UPPER_BOUND && eval > tt_score)
                        eval = tt_score;
                }
            } 
            else 
            {
                eval = ss->static_eval = Eval::evaluate(board);
                tt.store(hash, NO_MOVE, scoreToTT(eval, ss->ply), -1, NO_BOUND);
            }

            // stand pat
            if (eval >= beta) 
                return eval;
            if (eval > alpha) 
                alpha = eval;

            best_score = eval;
            futility = eval + qfp_margin;
        }

        MovePicker mp(Q_SEARCH, board, history, ss, ent.move, in_check, depth >= -1);

        Move best_move = NO_MOVE;
        Move move = NO_MOVE;

        while ((move = mp.nextMove(false)) != NO_MOVE)
        {
            if (best_score > -VALUE_TB_WIN_IN_MAX_PLY)
            {
                if (!in_check && futility <= alpha && board.isCapture(move) && !board.see(move, 1)) 
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
        }

        if (mp.getMoveCount() == 0) 
            return in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;

        if (best_score >= beta && abs(best_score) < VALUE_TB_WIN_IN_MAX_PLY)
            best_score = (best_score + beta) / 2;

        // store in transposition table
        Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND; // exact bounds can only be stored in pv search
        tt.store(hash, best_move, scoreToTT(best_score, ss->ply), 0, bound);
        
        return best_score;
    }

    void Search::updatePV(int ply, Move m)
    {
        PVLine& current_pv = pv_table[ply];
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

        const U64 elapsed_time = tm.elapsedTime();
        const U64 total_nodes = threads.getTotalNodes();
        const U64 nps = total_nodes * 1000 / (elapsed_time + 1);

        std::cout << " nodes " << total_nodes
                  << " nps " << nps
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
