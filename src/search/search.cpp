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
    PARAM(lmr_base, 95, 80, 130, 8);
    PARAM(lmr_div, 168, 150, 200, 8);

    PARAM(rzr_depth, 4, 3, 5, 1);
    PARAM(rzr_depth_mult, 186, 150, 250, 15);
    
    PARAM(rfp_depth, 9, 7, 11, 1);
    PARAM(rfp_depth_mult, 81, 50, 100, 5);

    PARAM(nmp_base, 4, 3, 5, 1);
    PARAM(nmp_depth_div, 5, 3, 6, 1);
    PARAM(nmp_min, 4, 3, 6, 1);
    PARAM(nmp_div, 214, 200, 220, 2);

    PARAM(see_cap_margin, 94, 85, 110, 3);
    PARAM(see_quiet_margin, 92, 75, 100, 3);

    PARAM(fp_base, 147, 120, 180, 10);
    PARAM(fp_mult, 101, 85, 110, 5);

    PARAM(asp_depth, 9, 6, 9, 1);
    PARAM(asp_window, 18, 10, 50, 5);

    PARAM(ch_mult, 12, 8, 16, 1);
    PARAM(hh_mult, 149, 140, 170, 5);

    // search helper

    Score drawScore(U64 nodes) { return VALUE_DRAW - 1 + Score(nodes & 0x2); }

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
        reset();
    }

    Move Search::start()
    {
        if (use_tb)
        {
            const auto dtz = probeDTZ(board);
            if (dtz.second != NO_MOVE)
            {
                threads.stop = true;

                if (id == 0)
                    std::cout << "bestmove " << dtz.second << std::endl;

                return dtz.second;
            }
        }

        if (id == 0)
            tt.incrementAge();

        Stack stack[MAX_PLY + 2];
        Stack *ss = stack + 2; // +2 to avoid stack underflow (history accesses ss - 2)

        // init stack
        for (int i = 2; i > 0; --i)
        {
            (ss - i)->ply = i;
            (ss - i)->current_move = NO_MOVE;
            (ss - i)->eval = 0;
            (ss - i)->excluded_move = NO_MOVE;
        }

        for (int i = 0; i < MAX_PLY; ++i)
        {
            (ss + i)->ply = i;
            (ss + i)->current_move = NO_MOVE;
            (ss + i)->eval = 0;
            (ss + i)->excluded_move = NO_MOVE;
        }

        time_manager.start();

        history.clear();
        history.init(hh_mult, ch_mult);

        Score previous_result = VALUE_NONE;

        Move best_move = NO_MOVE;
        for (int depth = 1; depth <= MAX_PLY; depth++)
        {
            pv_table.reset();

            const Score result = aspSearch(depth, previous_result, ss);
            previous_result = result;

            if (isLimitReached(depth))
                break;

            best_move = pv_table(0)(0);

            if (id == 0)
                printUciInfo(result, depth, pv_table(0));
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
            if (alpha < -500)
                alpha = -VALUE_INFINITE;
            
            if (beta > 500) 
                beta = VALUE_INFINITE;
            
            result = negamax(depth, alpha, beta, ROOT, ss);

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

            window += window / 2;
        }

        return result;
    }

    Score Search::negamax(int depth, Score alpha, Score beta, Node node, Stack *ss)
    {
        assert(alpha < beta);
        assert(ss->ply >= 0);

        if (isLimitReached(depth))
            return beta;

        if (ss->ply >= MAX_PLY)
            return Eval::evaluate(board);

        pv_table(ss->ply).length = ss->ply;

        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();

        const bool root_node = node == ROOT;
        const bool pv_node = node != NON_PV;

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
            // check for alpha cut-off from a theoretical mate position
            mating_value = -VALUE_MATE + ss->ply;
            if (mating_value > alpha)
            {
                alpha = mating_value;
                if (beta <= mating_value)
                    return mating_value; // alpha cut-off
            }

            if (board.isRepetition(pv_node))
                return drawScore(nodes);

            // check for draw
            if (board.isDraw())
                return VALUE_DRAW;

            // check for mate and stalemate
            MoveList ml(board);
            if (ml.size() == 0)
                return in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;
        }

        // check extension
        if (in_check)
            depth++;

        // quiescence search
        if (depth <= 0)
            return qSearch(alpha, beta, node, ss);

        (ss + 1)->excluded_move = NO_MOVE;

        // selective depth
        if (pv_node && ss->ply > sel_depth)
            sel_depth = ss->ply; // heighest depth a pv node has reached

        // look up in transposition table
        TTEntry ent;
        const U64 hash = board.getHash();
        const bool tt_hit = tt.lookup(ent, hash);
        const Score tt_score = tt_hit ? scoreFromTT(ent.score, ss->ply) : Score(VALUE_NONE);

        const Move excluded_move = ss->excluded_move;

        if (!root_node 
            && !pv_node 
            && !excluded_move 
            && tt_hit 
            && tt_score != VALUE_NONE 
            && ent.depth >= depth 
            && (ss - 1)->current_move != NULL_MOVE)
        {
            ss->eval = tt_score;

            switch (ent.bound)
            {
            case EXACT_BOUND:
                return tt_score;
            case LOWER_BOUND:
                alpha = std::max(alpha, tt_score);
                break;
            case UPPER_BOUND:
                beta = std::min(beta, tt_score);
                break;
            default:
                break;
            }

            // check for a cut-off
            if (alpha >= beta)
                return tt_score;
        }
        else
            ss->eval = in_check ? Score(VALUE_NONE) : Eval::evaluate(board);

        // use tt score for better evaluation
        if (tt_hit && ss->eval != VALUE_NONE)
        {
            if (ent.bound == EXACT_BOUND)
                ss->eval = tt_score;
            else if (ent.bound == LOWER_BOUND && ss->eval < tt_score)
                ss->eval = tt_score;
            else if (ent.bound == UPPER_BOUND && ss->eval > tt_score)
                ss->eval = tt_score;
        }
   
        Score max_score = VALUE_INFINITE;
        Score best_score = -VALUE_INFINITE;

        // tablebase probing
        if (use_tb && !root_node)
        {
            Score tb_score = probeWDL(board);

            if (tb_score != VALUE_NONE)
            {
                Bound bound = NO_BOUND;
                tb_hits++;

                switch (tb_score)
                {
                case VALUE_TB_WIN:
                    tb_score = VALUE_MIN_MATE - ss->ply - 1;
                    bound = LOWER_BOUND;
                    break;
                case VALUE_TB_LOSS:
                    tb_score = -VALUE_MIN_MATE + ss->ply + 1;
                    bound = UPPER_BOUND;
                    break;
                default:
                    tb_score = 0;
                    bound = EXACT_BOUND;
                    break;
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

        // check for improvement
        bool improving = false;
        if (!in_check && (ss - 2)->eval != VALUE_NONE)
            improving = ss->eval > (ss - 2)->eval;

        // only use pruning/reduction when not in check
        if (!in_check)
        {
            // internal iterative reduction
            if (depth >= 3 && !tt_hit && pv_node)
                depth--;

            // razoring
            if (!pv_node 
                && depth <= 3 
                && ss->eval + rzr_depth_mult * depth < alpha)
            {
                Score score = qSearch(alpha, beta, NON_PV, ss);
                if (score < alpha)
                    return score;
            }

            // reverse futility pruning
            if (!pv_node 
                && depth <= 7 
                && ss->eval < VALUE_TB_WIN_IN_MAX_PLY
                && ss->eval - rfp_depth_mult * (depth - improving) >= beta)
            {
                return (ss->eval + beta) / 2;
            }

            // null move pruning
            if (!pv_node 
                && depth >= 4  
                && board.nonPawnMat(stm) 
                && !excluded_move 
                && ss->eval >= beta
                && (ss - 1)->current_move != NULL_MOVE )
            {
                assert(ss->eval - beta >= 0);

                int R = 5 + depth / nmp_depth_div + std::min(int(nmp_min), (ss->eval - beta) / nmp_div);

                ss->current_move = NULL_MOVE;

                board.makeNullMove();
                Score score = -negamax(depth - R, -beta, -beta + 1, NON_PV, ss + 1);
                board.unmakeNullMove();

                if (score >= beta)
                {
                    // don't return unproven mate scores
                    if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                        return beta;

                    return score;
                }
            }

            // probcut
            int beta_cut = beta + 136;
            if (!pv_node 
                && depth > 3 
                && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY 
                && !(ent.depth >= depth - 3 && tt_score != VALUE_NONE && tt_score < beta_cut))
            {
                MovePicker movepicker(CAPTURE_MOVES, board, history, ss, ent.move);

                Move move = NO_MOVE;
                while ((move = movepicker.nextMove()) != NO_MOVE)
                {
                    if (move == ss->excluded_move)
                        continue;

                    nodes++;
                    board.makeMove(move, true);

                    Score score = -qSearch(-beta_cut, -beta_cut + 1, NON_PV, ss + 1);

                    if (score >= beta_cut)
                        score = -negamax(depth - 4, -beta_cut, -beta_cut + 1, NON_PV, ss + 1);

                    board.unmakeMove(move);

                    if (score >= beta_cut)
                    {
                        tt.store(hash, move, scoreToTT(score, ss->ply), depth - 3, LOWER_BOUND);
                        return beta_cut;
                    }
                }
            }
        }

        MovePicker movepicker(ALL_MOVES, board, history, ss, ent.move);

        uint8_t made_moves = 0, quiet_count = 0;

        Move quiet_moves[64];
        Move best_move = NO_MOVE, move = NO_MOVE;

        while ((move = movepicker.nextMove()) != NO_MOVE)
        {
            if (move == excluded_move)
                continue;

            made_moves++;

            int extension = 0;

            bool is_cap = board.isCapture(move);
            bool is_prom = isPromotion(move);

            if (!root_node && best_score > VALUE_TB_LOSS_IN_MAX_PLY)
            {
                assert(ss->eval != VALUE_NONE || in_check);

                // late move pruning
                if (!pv_node 
                    && !in_check 
                    && !is_cap 
                    && !is_prom 
                    && depth <= 5 
                    && quiet_count > (4 + depth * depth))
                {
                    continue;
                }

                // see pruning
                if (depth < (is_cap ? 6 : 7) && !board.see(move, -(is_cap ? 94 : 92) * depth))
                    continue;
            
                // futility pruning
                if (!pv_node 
                    && !in_check 
                    && !is_cap 
                    && !is_prom 
                    && depth <= 9 
                    && ss->eval + fp_base + depth * fp_mult <= alpha)
                {
                    continue;
                }
            }

            // print current move information
            if (id == 0 
                && root_node 
                && time_manager.elapsedTime() > 5000 
                && !threads.isStopped())
            {
                std::cout << "info depth " << depth
                          << " currmove " << move
                          << " currmovenumber " << int(made_moves) << std::endl;
            }

            // singular extensions
            if (!root_node 
                && depth >= 8 
                && tt_hit 
                && tt_score != VALUE_NONE 
                && ent.move == move 
                && !excluded_move 
                && std::abs(tt_score) < VALUE_TB_WIN_IN_MAX_PLY 
                && ent.bound & LOWER_BOUND 
                && ent.depth >= depth - 3)
            {
                const Score singular_beta = tt_score - 3 * depth;
                const int singular_depth = (depth - 1) / 2;

                ss->excluded_move = move;
                Score score = negamax(singular_depth, singular_beta - 1, singular_beta, NON_PV, ss);
                ss->excluded_move = NO_MOVE;

                if (score < singular_beta)
                    extension = 1;
                else if (singular_beta >= beta)
                    return singular_beta;
            }

            const int new_depth = depth - 1 + extension;

            nodes++;
            board.makeMove(move, true);

            ss->current_move = move;

            Score score = VALUE_NONE;

            // late move reduction
            if (depth >= 2 && !in_check && made_moves > 3)
            {
                int rdepth = REDUCTIONS[depth][made_moves];
                rdepth += improving;
                rdepth -= pv_node;
                rdepth -= is_cap;
                rdepth = std::clamp(new_depth - rdepth, 1, new_depth + 1);

                score = -negamax(rdepth, -alpha - 1, -alpha, NON_PV, ss + 1);

                // if late move reduction failed high, research
                if (score > alpha && rdepth < new_depth)
                    score = -negamax(new_depth, -alpha - 1, -alpha, NON_PV, ss + 1);
            }
            else if (!pv_node || made_moves > 1)
                // full-depth search if lmr was skipped
                score = -negamax(new_depth, -alpha - 1, -alpha, NON_PV, ss + 1);

            // principal variation search
            if (pv_node && ((score > alpha && score < beta) || made_moves == 1))
                score = -negamax(new_depth, -beta, -alpha, PV, ss + 1);

            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;

                    // update pv table
                    pv_table.updatePV(ss->ply, best_move);
                }

                if (score >= beta)
                {
                    history.update(board, move, quiet_moves, ss, quiet_count, depth);
                    // cut-off
                    break;
                }
            }

            if (!is_cap && !is_prom)
                quiet_moves[quiet_count++] = move;
        }

        // check for mate and stalemate
        if (movepicker.getMoveCount() == 0)
        {
            if (excluded_move != NO_MOVE)
                best_score = alpha;
            else
                best_score = in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;
        }

        if (pv_node)
            best_score = std::min(best_score, max_score);

        // store in transposition table
        if (!excluded_move && !threads.isStopped())
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

    Score Search::qSearch(Score alpha, Score beta, Node node, Stack *ss)
    {
        assert(alpha < beta);

        if (isLimitReached(1))
            return beta;

        if (board.isDraw())
            return VALUE_DRAW;

        if (ss->ply >= MAX_PLY)
            return Eval::evaluate(board);

        const bool pv_node = node == PV;
        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();

        if (board.isRepetition(pv_node))
            return drawScore(nodes);

        Score stand_pat, best_score;

        // look up in transposition table
        TTEntry ent;
        const U64 hash = board.getHash();
        const bool tt_hit = tt.lookup(ent, hash);
        const Score tt_score = tt_hit ? scoreFromTT(ent.score, ss->ply) : Score(VALUE_NONE);

        if (tt_hit 
            && !pv_node 
            && tt_score != VALUE_NONE 
            && ent.bound != NO_BOUND)
        {
            if (ent.bound == EXACT_BOUND)
                return tt_score;
            else if (ent.bound == LOWER_BOUND && tt_score >= beta)
                return tt_score;
            else if (ent.bound == UPPER_BOUND && tt_score <= alpha)
                return tt_score;

            stand_pat = tt_score;
            best_score = tt_score;
        }
        else
        {
            stand_pat = Eval::evaluate(board);
            best_score = stand_pat;
        }

        // use tt score for better evaluation
        if (tt_hit)
        {
            if (ent.bound == EXACT_BOUND)
                best_score = tt_score;
            else if (ent.bound == LOWER_BOUND && stand_pat < tt_score)
                best_score = tt_score;
            else if (ent.bound == UPPER_BOUND && stand_pat > tt_score)
                best_score = tt_score;
        }

        if (best_score >= beta)
            return best_score;

        if (best_score > alpha)
            alpha = best_score;

        MovePicker movepicker(CAPTURE_MOVES, board, history, ss, ent.move);

        Move best_move = NO_MOVE;
        Move move = NO_MOVE;

        while ((move = movepicker.nextMove()) != NO_MOVE)
        {
            if (best_score > VALUE_TB_LOSS_IN_MAX_PLY && !in_check)
            {
                // delta pruning
                PieceType captured = typeOf(board.pieceAt(move.to()));
                if (!isPromotion(move) 
                    && board.nonPawnMat(stm) 
                    && stand_pat + 400 + PIECE_VALUES[captured] < alpha)
                {
                    continue;
                }

                // see pruning
                if (!board.see(move, 0))
                    continue;
            }

            nodes++;

            board.makeMove(move, true);
            Score score = -qSearch(-beta, -alpha, node, ss + 1);
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
                    // cut-off
                    break;
            }
        }

        // store in transposition table
        if (!threads.isStopped())
        {
            // exact bounds can only be stored in pv search
            Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND;
            tt.store(hash, best_move, scoreToTT(best_score, ss->ply), 0, bound);
        }

        return best_score;
    }

    bool Search::isLimitReached(const int depth) const
    {
        if (limit.infinite)
            return false;

        if (threads.isStopped())
            return true;

        if (limit.time != 0 && time_manager.elapsedTime() > limit.time)
            return true;

        if (limit.nodes != 0 && nodes >= limit.nodes)
            return true;

        if (depth > limit.depth)
            return true;

        return false;
    }

    void Search::reset()
    {
        nodes = 0;
        tb_hits = 0;
        tt.clear();
        pv_table.reset();
        history.clear();
    }

    void Search::printUciInfo(Score result, int depth, PVLine &pv_line) const
    {
        std::cout << "info depth " << depth
                  << " seldepth " << int(threads.getSelDepth())
                  << " score ";

        if (abs(result) >= VALUE_MIN_MATE)
            std::cout << "mate " << (VALUE_MATE - abs(result) + 1) / 2 * (result > 0 ? 1 : -1);
        else
            std::cout << "cp " << result;

        const U64 elapsed_time = time_manager.elapsedTime();
        const U64 total_nodes = threads.getTotalNodes();
        const U64 nps = total_nodes * 1000 / (elapsed_time + 1);

        std::cout << " nodes " << total_nodes
                  << " nps " << nps
                  << " tbhits " << threads.getTotalTbHits()
                  << " time " << elapsed_time
                  << " pv";

        // print the pv
        for (int i = 0; i < pv_line.length; i++)
            std::cout << " " << pv_line(i);

        std::cout << std::endl;
    }

    // global variables
    TTable tt(16);

} // namespace Astra
