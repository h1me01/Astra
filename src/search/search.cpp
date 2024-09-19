#include <cmath>
#include <algorithm>
#include "search.h"
#include "../eval/eval.h"
#include "../chess/movegen.h"
#include "../syzygy/tbprobe.h"

namespace Astra {

    constexpr int RAZOR_MARGIN = 129;
    constexpr int DELTA_MARGIN = 400;

    int REDUCTIONS[MAX_PLY][MAX_MOVES];

    void initReductions() {
        REDUCTIONS[0][0] = 0;

        for (int depth = 1; depth < MAX_PLY; depth++) {
            for (int moves = 1; moves < MAX_MOVES; moves++) {
                REDUCTIONS[depth][moves] = 1.25 + log(depth) * log(moves) / 2;
            }
        }
    }

    Score scoreToTT(Score s, int ply) {
        if (s >= VALUE_TB_WIN_IN_MAX_PLY) {
            return s + ply;
        }
        if (s <= VALUE_TB_LOSS_IN_MAX_PLY) {
            return s - ply;
        }
        return s;
    }

    Score scoreFromTT(Score s, int ply) {
        if (s == VALUE_NONE) {
            return VALUE_NONE;
        }
        if (s >= VALUE_TB_WIN_IN_MAX_PLY) {
            return s - ply;
        }
        if (s <= VALUE_TB_LOSS_IN_MAX_PLY) {
            return s + ply;
        }
        return s;
    }

    Score probeWDL(Board &board) {
        U64 w_occ = board.occupancy(WHITE);
        U64 b_occ = board.occupancy(BLACK);
        U64 occ = w_occ | b_occ;

        // can't probe if there are more pieces than supported
        if (popCount(occ) > static_cast<signed>(TB_LARGEST)) {
            return VALUE_NONE;
        }

        U64 pawns = board.getPieceBB(WHITE, PAWN) | board.getPieceBB(BLACK, PAWN);
        U64 knights = board.getPieceBB(WHITE, KNIGHT) | board.getPieceBB(BLACK, KNIGHT);
        U64 bishops = board.getPieceBB(WHITE, BISHOP) | board.getPieceBB(BLACK, BISHOP);
        U64 rooks = board.getPieceBB(WHITE, ROOK) | board.getPieceBB(BLACK, ROOK);
        U64 queens = board.getPieceBB(WHITE, QUEEN) | board.getPieceBB(BLACK, QUEEN);

        int fifty_move_counter = board.history[board.getPly()].half_move_clock;
        bool any_castling = popCount(board.history[board.getPly()].castle_mask) != 6;
        Square ep_sq = board.history[board.getPly()].ep_sq;
        bool white_to_move = board.getTurn() == WHITE;

        unsigned result = tb_probe_wdl(occ, w_occ, b_occ, pawns, knights, bishops, rooks, queens, fifty_move_counter, any_castling, ep_sq, white_to_move);

        if (result == TB_LOSS) {
            return VALUE_TB_LOSS;
        }
        if (result == TB_WIN) {
            return VALUE_TB_WIN;
        }
        if (result == TB_DRAW || result == TB_BLESSED_LOSS || result == TB_CURSED_WIN) {
            return VALUE_DRAW;
        }

        // if none of them above happened, act as if the result failed.
        return VALUE_NONE;
    }

    Search::Search(const std::string &fen) : board(fen), tt(16) {
        reset();
    }

    Score Search::qSearch(Score alpha, Score beta, Node node, Stack *ss) {
        if (board.isDraw()) {
            return VALUE_DRAW;
        }

        if (ss->ply >= MAX_PLY) {
            return Eval::evaluate(board);
        }

        const bool pv_node = node == PV;
        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();

        assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
        assert(pv_node || (alpha == beta - 1));

        // look up in transposition table
        TTEntry tt_entry;
        const U64 hash = board.getHash();
        const bool tt_hit = tt.lookup(tt_entry, hash);
        const Score tt_score = tt_hit ? scoreFromTT(tt_entry.score, ss->ply) : VALUE_NONE;

        if (tt_hit && !pv_node && tt_score != VALUE_NONE) {
            Bound tt_bound = tt_entry.bound;

            if (tt_bound == EXACT_BOUND) {
                return tt_score;
            }
            if (tt_bound == LOWER_BOUND && tt_score >= beta) {
                return tt_score;
            }
            if (tt_bound == UPPER_BOUND && tt_score <= alpha) {
                return tt_score;
            }
        }

        Score best_score = Eval::evaluate(board);

        if (best_score >= beta) {
            return best_score;
        }
        if (best_score > alpha) {
            alpha = best_score;
        }

        MoveList moves(board);
        move_ordering.sortMoves(board, moves, tt, (ss - 1)->current_move, ss->ply);

        Move best_move = NO_MOVE;
        for (Move move: moves) {
            if (!isCapture(move)) {
                // don't search for non captures
                continue;
            }

            if (best_score > VALUE_TB_LOSS_IN_MAX_PLY && !in_check) {
                // delta pruning
                PieceType captured = typeOf(board.pieceAt(move.to()));
                if (best_score + DELTA_MARGIN + PIECE_VALUES[captured] < alpha && !isPromotion(move) && board.nonPawnMat(stm)) {
                    continue;
                }

                // see pruning
                if (!see(board, move, 0)) {
                    continue;
                }
            }

            nodes++;

            board.makeMove(move, true);
            Score score = -qSearch(-beta, -alpha, node, ss + 1);
            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            // update the best score
            if (score > best_score) {
                best_score = score;

                if (score > alpha) {
                    alpha = score;
                    best_move = move;

                    if (score >= beta) {
                        // beta cutoff
                        break;
                    }
                }
            }
        }

        // check for mate and stalemate
        if (moves.size() == 0) {
            best_score = in_check ? -VALUE_MATE + ss->ply : 0;
        }

        // store in transposition table
        if (best_move != NULL_MOVE) {
            Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND;
            tt.store(hash, best_move, scoreToTT(best_score, ss->ply), 0, bound);
        }

        assert(best_score > -VALUE_INFINITE && best_score < VALUE_INFINITE);
        return best_score;
    }

    Score Search::abSearch(int depth, Score alpha, Score beta, Node node, Stack *ss) {
        //if (time_manager.isTimeExceeded()) {
          // return 0;
        //}

        if (ss->ply >= MAX_PLY) {
            return Eval::evaluate(board);
        }

        if (board.isDraw()) {
            return VALUE_DRAW;
        }

        pv_table(ss->ply).length = ss->ply;

        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();

        const bool root_node = node == ROOT;
        const bool pv_node = node != NON_PV;

        // check extension
        if (in_check) {
            depth++;
        }

        // quiescence search
        if (depth <= 0) {
            return qSearch(alpha, beta, node, ss);
        }

        // safety checks
        assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
        assert(pv_node || (alpha == beta - 1));
        assert(0 < depth && depth < MAX_PLY);
        assert(ss->ply < MAX_PLY);

        (ss + 1)->excluded_move = NO_MOVE;

        // Look up in the TT
        TTEntry tt_entry;
        const U64 hash = board.getHash();
        const bool tt_hit = tt.lookup(tt_entry, hash);
        const Score tt_score = tt_hit ? scoreFromTT(tt_entry.score, ss->ply) : VALUE_NONE;

        const Move excluded_move = ss->excluded_move;

        // look up in transposition table
        if (!root_node && !pv_node && excluded_move != NULL_MOVE
            && tt_hit && tt_score != VALUE_NONE && tt_entry.depth >= depth
            && (ss - 1)->current_move != NULL_MOVE) {
            switch (tt_entry.bound) {
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

            if (alpha >= beta) {
                return tt_score;
            }
        }

        // tablebase probing
        if (use_TB) {
            Score tb_score = probeWDL(board);
            if (tb_score != VALUE_NONE) {
                std::cout << "TB score: " << tb_score << std::endl;
                switch (tb_score) {
                    case VALUE_TB_WIN:
                        tb_score = VALUE_MATE_IN_PLY - ss->ply - 1;
                        break;
                    case VALUE_TB_LOSS:
                        tb_score = VALUE_MATED_IN_PLY + ss->ply + 1;
                        break;
                    default:
                        tb_score = 0;
                        break;
                }

                return tb_score;
            }
        }

        bool is_improving = false;

        if (in_check) {
            ss->eval = VALUE_NONE;
        } else {
            ss->eval = tt_hit && tt_score != VALUE_NONE ? tt_score : Eval::evaluate(board);
            is_improving = (ss - 2)->eval != VALUE_NONE && ss->eval > (ss - 2)->eval;
        }

        if (!in_check && !root_node) {
            // internal iterative reductions
            if (depth >= 3 && !tt_hit) {
                depth--;
            }
            if (pv_node && !tt_hit) {
                depth--;
            }
            if (depth <= 0) {
                return qSearch(alpha, beta, PV, ss);
            }

            // razoring
            if (!pv_node && depth < 3 && ss->eval + RAZOR_MARGIN < alpha) {
                return qSearch(alpha, beta, NON_PV, ss);
            }

            // null move pruning
            if (!pv_node && board.nonPawnMat(stm) && excluded_move != NULL_MOVE && (ss - 1)->current_move != NULL_MOVE && depth >= 3 && ss->eval >= beta) {
                constexpr int R = 5;

                ss->current_move = NULL_MOVE;

                board.makeNullMove();
                Score score = -abSearch(depth - R, -beta, -beta + 1, NON_PV, ss + 1);
                board.unmakeNullMove();

                if (score >= beta) {
                    return score;
                }
            }
        }

        // mate distance pruning
        Score mating_value = VALUE_MATE - ss->ply;
        if (mating_value < beta) {
            beta = mating_value;
            if (alpha >= mating_value) {
                return mating_value; // beta cutoff
            }
        }
        // check for alpha cutoff from a theoretical mate position
        mating_value = -VALUE_MATE + ss->ply;
        if (mating_value > alpha) {
            alpha = mating_value;
            if (beta <= mating_value) {
                return mating_value; // alpha cutoff
            }
        }

        MoveList moves(board);
        move_ordering.sortMoves(board, moves, tt, (ss - 1)->current_move, ss->ply);

        Score highest_score = -VALUE_INFINITE;
        Score score = VALUE_NONE;

        Move best_move = NO_MOVE;

        bool do_full_search = false;

        ss->move_count = moves.size();

        uint8_t quiet_count = 0;
        uint8_t made_moves = 0;
        for (Move move: moves) {
            if (move == excluded_move) {
                continue;
            }

            made_moves++;

            int extension = 0;

            const bool is_capture = isCapture(move);
            const bool is_promotion = isPromotion(move);

            if (!root_node && highest_score > VALUE_TB_LOSS_IN_MAX_PLY) {
                if (is_capture) {
                    // see pruning
                    if (depth < 6 && !see(board, move, 0)) {
                        continue;
                    }
                } else {
                    //
                    if (!in_check && !pv_node && !is_promotion && depth <= 5 && quiet_count > (4 + depth * depth)) {
                        continue;
                    }

                    // see pruning
                    if (depth < 7 && !see(board, move, 0)) {
                        continue;
                    }
                }
            }

            // one reply extension
            if (in_check && (ss - 1)->move_count == 1 && ss->move_count == 1) {
                extension += 1;
            }

            const int newDepth = depth - 1 + extension;

            nodes++;
            board.makeMove(move, true);

            ss->current_move = move;

            // late move reduction
            if (depth > 2 && !in_check && made_moves > 3) {
                int rdepth = REDUCTIONS[depth][made_moves];
                rdepth += is_improving;
                rdepth -= pv_node;
                rdepth -= is_capture;
                rdepth = std::clamp(newDepth - rdepth, 1, newDepth + 1);

                score = -abSearch(rdepth, -alpha - 1, -alpha, NON_PV, ss + 1);
                do_full_search = score > alpha && rdepth < newDepth;
            } else {
                do_full_search = !pv_node || made_moves > 1;
            }

            if (do_full_search) {
                score = -abSearch(newDepth, -alpha - 1, -alpha, NON_PV, ss + 1);
            }

            // principal variation search
            if (pv_node && ((score > alpha && score < beta) || made_moves == 1)) {
                score = -abSearch(newDepth, -beta, -alpha, PV, ss + 1);
            }

            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            if (score > highest_score) {
                highest_score = score;

                if (score > alpha) {
                    alpha = score;
                    best_move = move;

                    // don't forget to update pv table
                    pv_table.updatePV(ss->ply, move);

                    if (score >= beta) {
                        if (!is_capture && !is_promotion) {
                            move_ordering.updateKiller(move, ss->ply);
                            move_ordering.updateCounters(move, (ss - 1)->current_move);
                            move_ordering.updateHistory(board, move, std::min(2000, depth * 155));
                        }

                        // beta cutoff
                        break;
                    }
                }
            }

            if (!is_capture && !is_promotion) {
                quiet_count++;
            }
        }

        // check for mate and stalemate
        if (moves.size() == 0) {
            highest_score = in_check ? -VALUE_MATE + ss->ply : 0;
        }

        ss->move_count = made_moves;

        // store in transposition table
        if (excluded_move != NULL_MOVE) {
            Bound bound;
            if (highest_score >= beta) {
                bound = LOWER_BOUND;
            } else if (pv_node && best_move != NO_MOVE) {
                bound = EXACT_BOUND;
            } else {
                bound = UPPER_BOUND;
            }

            tt.store(hash, best_move, scoreToTT(highest_score, ss->ply), depth, bound);
        }

        assert(highest_score > -VALUE_INFINITE && highest_score < VALUE_INFINITE);
        return highest_score;
    }

    Score Search::aspSearch(int depth, Score prev_eval, Stack *ss) {
        Score alpha = -VALUE_INFINITE;
        Score beta = VALUE_INFINITE;

        // only use aspiration window when depth is higher or equal to 9
        int delta = 30;
        if (depth >= 9) {
            alpha = prev_eval - delta;
            beta = prev_eval + delta;
        }

        Score result = VALUE_NONE;
        while (true) {
            if (alpha < -3500) {
                alpha = -VALUE_INFINITE;
            }

            if (beta > 3500) {
                beta = VALUE_INFINITE;
            }

            result = abSearch(depth, alpha, beta, ROOT, ss);

            if (time_manager.isTimeExceeded()) {
                return result;
            }

            if (result <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max(alpha - delta, -(static_cast<int>(VALUE_INFINITE)));
                delta += delta / 2;
            } else if (result >= beta) {
                beta = std::min(beta + delta, static_cast<int>(VALUE_INFINITE));
                delta += delta / 2;
            } else {
                break;
            }
        }

        return result;
    }

    SearchResult Search::bestMove(int max_depth, unsigned int time_per_move) {
        SearchResult search_result;

        Stack stack[MAX_PLY + 4], *ss = stack + 2;
        for (int i = 2; i > 0; --i) {
            (ss - i)->ply = i;
            (ss - i)->move_count = 0;
            (ss - i)->current_move = NO_MOVE;
            (ss - i)->eval = 0;
            (ss - i)->excluded_move = NO_MOVE;
        }

        for (int i = 0; i <= MAX_PLY + 1; ++i) {
            (ss + i)->ply = i;
            (ss + i)->move_count = 0;
            (ss + i)->current_move = NO_MOVE;
            (ss + i)->eval = 0;
            (ss + i)->excluded_move = NO_MOVE;
        }

        time_manager.setTimePerMove(time_per_move);
        time_manager.start();

        pv_table.reset();

        int depth = 1;
        for (; depth <= max_depth; depth++) {
            const Score previous_result = search_result.score;
            const Score result = aspSearch(depth, previous_result, ss);

            if (time_manager.isTimeExceeded()) {
                break;
            }

            search_result.best_move = pv_table(0)(0);
            search_result.score = result;

            // info for uci
            std::cout << "info depth " << depth
                    << " score ";

            Score best_score = search_result.score;
            if (best_score >= VALUE_MATE_IN_PLY) {
                std::cout << "mate " << (VALUE_MATE - best_score) / 2 + ((VALUE_MATE - best_score) & 1);
            } else if (best_score <= VALUE_MATED_IN_PLY) {
                std::cout << "mate " << -((VALUE_MATE + best_score) / 2) + ((VALUE_MATE + best_score) & 1);
            } else {
                std::cout << "cp " << best_score;
            }

            int elapsed_time = time_manager.elapsedTime();

            std::cout << " nodes " << nodes
                    << " nps " << nodes * 1000 / (elapsed_time + 1)
                    << " time " << elapsed_time;

            std::cout << " pv" << getPv() << std::endl;
        }

        if (depth == 1) {
            search_result.best_move = pv_table(0)(0);
        }

        return search_result;
    }

    void Search::reset() {
        board = Board(STARTING_FEN);
        nodes = 0;
        tt.clear();
        pv_table.reset();
        move_ordering.clear();
    }

    std::string Search::getPv() {
        std::ostringstream pv;

        for (int i = 0; i < pv_table(0).length; i++) {
            pv << " " << pv_table(0)(i);
        }

        return pv.str();
    }

} // namespace Astra
