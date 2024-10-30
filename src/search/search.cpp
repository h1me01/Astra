#include <cmath>
#include <algorithm>
#include "search.h"
#include "syzygy.h"
#include "tune.h"
#include "../eval/eval.h"

namespace Astra
{
   // search parameters
    PARAM(lmr_base, 100, 50, 150);
    PARAM(lmr_div, 175, 150, 250);
    PARAM(lmr_depth, 2, 1, 4);
    PARAM(lmr_min_moves, 3, 1, 5);

    PARAM(delta_margin, 450, 400, 900);

    PARAM(iir_depth, 3, 2, 4);

    PARAM(razor_margin, 130, 60, 200);
    PARAM(rzr_depth, 3, 2, 7);

    PARAM(rfp_depth_mult, 46, 20, 80);
    PARAM(rfp_impr_bonus, 50, 30, 100);
    PARAM(rfp_depth, 5, 3, 9);

    PARAM(snmp_margin, 100, 50, 150);
    PARAM(snmp_depth, 5, 2, 9);

    PARAM(nmp_depth, 3, 2, 5);

    PARAM(pv_see_cap_margin, 97, 90, 110);
    PARAM(pv_see_cap_depth, 6, 5, 8);

    PARAM(pv_see_quiet_margin, 69, 30, 95);
    PARAM(pv_see_quiet_depth, 7, 6, 9);

    PARAM(lmp_depth, 6, 4, 7);
    PARAM(lmp_count_base, 4, 3, 6);

    PARAM(history_bonus, 155, 100, 200);

    PARAM(asp_window, 30, 10, 50);

    // search class
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

    Search::Search(const std::string& fen) : board(fen)
    {
        reset();
    }

    Score Search::qSearch(Score alpha, Score beta, Node node, Stack* ss)
    {
        if (isLimitReached(1))
            return beta;

        if (board.isDraw())
            return VALUE_DRAW;

        if (ss->ply >= MAX_PLY)
            return Eval::evaluate(board);

        const bool pv_node = node == PV;
        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();

        assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
        assert(pv_node || (alpha == beta - 1));

        // look up in transposition table
        TTEntry tt_entry;
        const U64 hash = board.getHash();
        const bool tt_hit = tt.lookup(tt_entry, hash);
        const Score tt_score = tt_hit ? scoreFromTT(tt_entry.score, ss->ply) : static_cast<Score>(VALUE_NONE);
        Bound tt_bound = tt_entry.bound;

        if (tt_hit
            && !pv_node
            && tt_score != VALUE_NONE
            && tt_bound != NO_BOUND)
        {
            bool is_exact = tt_bound == EXACT_BOUND;
            bool is_lower = tt_bound == LOWER_BOUND && tt_score >= beta;
            bool is_upper = tt_bound == UPPER_BOUND && tt_score <= alpha;

            if (is_exact || is_lower || is_upper)
                return tt_score;
        }

        Score best_score = Eval::evaluate(board);

        if (best_score >= beta)
            return best_score;

        if (best_score > alpha)
            alpha = best_score;

        MoveList moves(board, CAPTURE_MOVES);
        move_ordering.sortMoves(board, moves, tt_entry.move, (ss - 1)->current_move, ss->ply);

        Move best_move = NO_MOVE;
        for (Move move : moves)
        {
            assert(isCapture(move));

            if (best_score > VALUE_TB_LOSS_IN_MAX_PLY && !in_check)
            {
                // delta pruning
                PieceType captured = typeOf(board.pieceAt(move.to()));
                if (captured != NO_PIECE_TYPE &&
                    best_score + delta_margin + PIECE_VALUES[captured] < alpha
                    && !isPromotion(move)
                    && board.nonPawnMat(stm))
                {
                    continue;
                }

                // see pruning
                if (!see(board, move, 0))
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
        if (!threads.stop)
        {
            // exact bounds can only be stored in pv search
            Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND;
            tt.store(hash, best_move, scoreToTT(best_score, ss->ply), 0, bound);
        }

        assert(best_score > -VALUE_INFINITE && best_score < VALUE_INFINITE);
        return best_score;
    }

    Score Search::abSearch(int depth, Score alpha, Score beta, Node node, Stack* ss)
    {
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

            // check for draw
            if (board.isDraw())
                return VALUE_DRAW;
        }

        // check extension
        if (in_check)
            depth++;

        // quiescence search
        if (depth <= 0)
            return qSearch(alpha, beta, node, ss);

        // safety checks
        assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
        assert(pv_node || (alpha == beta - 1));
        assert(0 < depth && depth < MAX_PLY);
        assert(ss->ply < MAX_PLY);

        (ss + 1)->excluded_move = NO_MOVE;

        // selective depth
        if (pv_node && ss->ply > sel_depth)
            sel_depth = ss->ply; // heighest depth a pv node has reached

        // look up in transposition table
        TTEntry tt_entry;
        const U64 hash = board.getHash();
        const bool tt_hit = tt.lookup(tt_entry, hash);
        const Score tt_score = tt_hit ? scoreFromTT(tt_entry.score, ss->ply) : static_cast<Score>(VALUE_NONE);

        const Move excluded_move = ss->excluded_move;

        if (!root_node
            && !pv_node
            && excluded_move == NO_MOVE
            && tt_hit
            && tt_score != VALUE_NONE
            && tt_entry.depth >= depth
            && (ss - 1)->current_move != NULL_MOVE)
        {
            switch (tt_entry.bound)
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

        bool is_improving = false;

        if (in_check)
            ss->static_eval = VALUE_NONE;
        else
        {
            ss->static_eval = tt_hit && tt_score != VALUE_NONE ? tt_score : Eval::evaluate(board);
            is_improving = ss->static_eval > (ss - 2)->static_eval;
        }

        // only use pruning/reduction when not in check and root/pv node
        if (!in_check && !root_node)
        {
            // internal iterative reductions
            if (depth >= iir_depth && !tt_hit)
                depth--;
            if (pv_node && !tt_hit)
                depth--;
            if (depth <= 0)
                return qSearch(alpha, beta, PV, ss);

            // razoring
            if (!pv_node
                && depth <= rzr_depth
                && ss->static_eval + razor_margin < alpha)
            {
                return qSearch(alpha, beta, NON_PV, ss);
            }

            // reverse futility pruning
            int rfp_margin = ss->static_eval - rfp_depth_mult * depth + rfp_impr_bonus * is_improving;
            if (!pv_node
                && depth < rfp_depth
                && rfp_margin >= beta
                && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY)
            {
                return beta;
            }

            // static null move pruning
            /*
            if (depth <= snmp_depth 
                && ss->static_eval >= beta + depth * snmp_margin
                && ss->static_eval < VALUE_MIN_MATE)
                return ss->static_eval;
            */

            // null move pruning
            if (!pv_node
                && board.nonPawnMat(stm)
                && excluded_move == NO_MOVE
                && (ss - 1)->current_move != NULL_MOVE
                && depth >= nmp_depth
                && ss->static_eval >= beta)
            {
                constexpr int R = 5;

                ss->current_move = NULL_MOVE;

                board.makeNullMove();
                Score score = -abSearch(depth - R, -beta, -beta + 1, NON_PV, ss + 1);
                board.unmakeNullMove();

                if (score >= beta)
                {
                    // don't return unproven mate scores
                    if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                        return beta;

                    return score;
                }
            }
        }

        MoveList moves(board);
        move_ordering.sortMoves(board, moves, tt_entry.move, (ss - 1)->current_move, ss->ply);

        Score score;
        Move best_move = NO_MOVE;

        ss->move_count = moves.size();

        uint8_t made_moves = 0, quiet_count = 0;
        for (Move move : moves)
        {
            if (move == excluded_move)
                continue;

            made_moves++;

            // print current move information
            if (id == 0
                && root_node
                && time_manager.elapsedTime() > 5000
                && !threads.stop)
            {
                std::cout << "info depth " << depth
                    << " currmove " << move
                    << " currmovenumber " << static_cast<int>(made_moves) << std::endl;
            }

            int extension = 0;

            const bool is_capture = isCapture(move);
            const bool is_promotion = isPromotion(move);

            if (!root_node && best_score > VALUE_TB_LOSS_IN_MAX_PLY)
            {
                int see_depth = is_capture ? pv_see_cap_depth : pv_see_quiet_depth;
                int see_margin = is_capture ? pv_see_cap_margin : pv_see_quiet_margin;

                // late move pruning
                if (!is_capture
                    && !in_check
                    && !pv_node
                    && !is_promotion
                    && depth <= lmp_depth
                    && quiet_count > (lmp_count_base + depth * depth))
                {
                    continue;
                }

                // see pruning
                if (depth < see_depth && !see(board, move, -see_margin * depth))
                    continue;
            }

            // singular extensions
            if (!root_node
                && depth >= 8
                && tt_hit
                && tt_score != VALUE_NONE
                && tt_entry.move == move
                && excluded_move == NO_MOVE
                && std::abs(tt_score) < VALUE_TB_WIN_IN_MAX_PLY
                && tt_entry.bound & LOWER_BOUND
                && tt_entry.depth >= depth - 3)
            {
                const Score singular_beta = tt_score - 3 * depth;
                const int singular_depth = (depth - 1) / 2;

                ss->excluded_move = move;
                const auto value = abSearch(singular_depth, singular_beta - 1, singular_beta, NON_PV, ss);
                ss->excluded_move = NO_MOVE;

                if (value < singular_beta)
                    extension = 1;
                else if (singular_beta >= beta)
                    return singular_beta;
            }

            // one reply extension
            if (in_check && (ss - 1)->move_count == 1 && ss->move_count == 1)
                extension += 1;

            const int new_depth = depth - 1 + extension;

            nodes++;
            board.makeMove(move, true);

            ss->current_move = move;

            if (made_moves == 1) 
            {
                score = -abSearch(new_depth, -beta, -alpha, PV, ss + 1);
            }
            else
            {
                // late move reduction
                if (depth >= lmr_depth && !in_check && made_moves > lmr_min_moves)
                {
                    int rdepth = REDUCTIONS[depth][made_moves];
                    rdepth += is_improving;
                    rdepth -= pv_node;
                    rdepth -= is_capture;
                    rdepth = std::clamp(new_depth - rdepth, 1, new_depth + 1);

                    score = -abSearch(rdepth, -alpha - 1, -alpha, NON_PV, ss + 1);

                    // if late move reduction failed high, re-search
                    if (score > alpha && rdepth < new_depth)
                        score = -abSearch(new_depth, -alpha - 1, -alpha, PV, ss + 1);
                }
                else if (!pv_node || made_moves > 1)
                    // full-depth search if lmr was skipped
                    score = -abSearch(new_depth, -alpha - 1, -alpha, NON_PV, ss + 1);

                // re-search
                if (score > alpha && score < beta)
                    score = -abSearch(new_depth, -beta, -alpha, PV, ss + 1);
            }

            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            if (score > best_score)
            {
                best_score = score;

                if (score > alpha)
                {
                    alpha = score;
                    best_move = move;

                    // don't forget to update pv table
                    pv_table.updatePV(ss->ply, move);
                }

                if (score >= beta)
                {
                    move_ordering.update(board, move, (ss - 1)->current_move, std::min(2000, depth * history_bonus), ss->ply);
                    // cut-off
                    break;
                }
            }

            if (!is_capture && !is_promotion)
                quiet_count++;
        }

        ss->move_count = made_moves;

        // check for mate and stalemate
        if (moves.size() == 0) 
        {
            if (excluded_move != NO_MOVE)
                best_score = alpha;
            else
                best_score = in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;
        }

        if (pv_node)
            best_score = std::min(best_score, max_score);

        // store in transposition table
        if (excluded_move == NO_MOVE && !threads.stop)
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

        assert(best_score > -VALUE_INFINITE && best_score < VALUE_INFINITE);
        return best_score;
    }

    Score Search::aspSearch(int depth, Score prev_eval, Stack* ss)
    {
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;

        // only use aspiration window when depth is higher or equal to 9
        int delta = asp_window;
        if (depth >= 9)
        {
            alpha = prev_eval - delta;
            beta = prev_eval + delta;
        }

        Score result = VALUE_NONE;
        while (true)
        {
            result = abSearch(depth, alpha, beta, ROOT, ss);

            if (isLimitReached(depth))
                return result;

            if (result <= alpha)
            {
                beta = (alpha + beta) / 2;
                alpha = std::max(alpha - delta, -static_cast<int>(VALUE_INFINITE));
                delta += delta / 2;
            }
            else if (result >= beta)
            {
                beta = std::min(beta + delta, static_cast<int>(VALUE_INFINITE));
                delta += delta / 2;
            }
            else
                break;
        }

        return result;
    }

    SearchResult Search::start()
    {
        if (use_tb)
        {
            const auto dtz = probeDTZ(board);
            if (dtz.second != NO_MOVE)
            {
                threads.stop = true;

                if (id == 0)
                    std::cout << "bestmove " << dtz.second << std::endl;
                
                return {dtz.second, dtz.first};
            }
        }

        if (id == 0)
            tt.incrementAge();

        Stack stack[MAX_PLY + 4];
        Stack* ss = stack + 2;

        // init stack
        for (int i = 2; i > 0; --i)
        {
            (ss - i)->ply = i;
            (ss - i)->move_count = 0;
            (ss - i)->current_move = NO_MOVE;
            (ss - i)->static_eval = 0;
            (ss - i)->excluded_move = NO_MOVE;
        }

        for (int i = 0; i <= MAX_PLY + 1; ++i)
        {
            (ss + i)->ply = i;
            (ss + i)->move_count = 0;
            (ss + i)->current_move = NO_MOVE;
            (ss + i)->static_eval = 0;
            (ss + i)->excluded_move = NO_MOVE;
        }

        time_manager.start();

        move_ordering.clear();

        Score previous_result = VALUE_NONE;

        SearchResult search_result;
        for (int depth = 1; depth <= MAX_PLY; depth++)
        {
            pv_table.clear();

            const Score result = aspSearch(depth, previous_result, ss);
            previous_result = result;

            if (isLimitReached(depth))
                break;

            search_result.best_move = pv_table(0)(0);
            search_result.score = result;

            if (id == 0)
                printUciInfo(result, depth, pv_table(0));
        }

        if (id == 0)
            std::cout << "bestmove " << search_result.best_move << std::endl;

        return search_result;
    }

    bool Search::isLimitReached(const int depth) const
    {
        if (threads.stop)
            return true;

        if (limit.infinite)
            return false;

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
        board = Board(STARTING_FEN);
        nodes = 0;
        tb_hits = 0;
        tt.clear();
        pv_table.clear();
        move_ordering.clear();
    }

    void Search::stop()
    {
        limit.depth = 0;
        limit.nodes = 0;
        limit.time = 0;
        limit.infinite = false;
    }

    void Search::printUciInfo(Score result, int depth, PVLine& pv_line) const
    {
        std::cout << "info depth " << depth
            << " seldepth " << static_cast<int>(threads.getSelDepth())
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

        // print the rest of the pv
        for (int i = 0; i < pv_line.length; i++)
            std::cout << " " << pv_line(i);

        std::cout << std::endl;
    }

    // class ThreadPool
    void ThreadPool::launchWorkers(const Board& board, const Limits& limit, int worker_count, bool use_tb)
    {
        stop = false;

        threads.clear();
        for (int i = 0; i < worker_count; i++)
        {
            Search thread(STARTING_FEN);
            thread.id = i;
            thread.board = board;
            thread.limit = limit;
            thread.use_tb = use_tb;
            threads.emplace_back(thread);
        }

        for (int i = 0; i < worker_count; i++)
            running_threads.emplace_back(&Search::start, std::ref(threads[i]));
    }

    void ThreadPool::stopAll()
    {
        stop = true;

        for (auto& th : running_threads)
            if (th.joinable())
                th.join();

        threads.clear();
        running_threads.clear();
    }

    U64 ThreadPool::getTotalNodes() const
    {
        U64 total_nodes = 0;
        for (const auto& t : threads)
            total_nodes += t.nodes;
        return total_nodes;
    }

    U64 ThreadPool::getTotalTbHits() const
    {
        U64 total_tb_hits = 0;
        for (const auto& t : threads)
            total_tb_hits += t.tb_hits;
        return total_tb_hits;
    }

    uint8_t ThreadPool::getSelDepth() const
    {
        uint8_t max_sel_depth = 0;
        for (const auto& t : threads)
            max_sel_depth = std::max(max_sel_depth, t.sel_depth);
        return max_sel_depth;
    }

    // global variables
    ThreadPool threads;
    TTable tt(16);

} // namespace Astra
