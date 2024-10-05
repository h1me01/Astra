#include <cmath>
#include <algorithm>
#include "search.h"
#include "../eval/eval.h"
#include "../chess/movegen.h"
#include "../syzygy/tbprobe.h"

namespace Astra {

    constexpr int RAZOR_MARGIN = 130;
    constexpr int DELTA_MARGIN = 400;

    int REDUCTIONS[MAX_PLY][MAX_MOVES];

    void initReductions() {
        REDUCTIONS[0][0] = 0;

        for (int depth = 1; depth < MAX_PLY; depth++) {
            for (int moves = 1; moves < MAX_MOVES; moves++) {
                REDUCTIONS[depth][moves] = 1.0 + log(depth) * log(moves) / 1.75;
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
        U64 kings = board.getPieceBB(WHITE, KING) | board.getPieceBB(BLACK, KING);

        int fifty_move_counter = board.history[board.getPly()].half_move_clock;
        bool any_castling = popCount(board.history[board.getPly()].castle_mask) != 6;
        Square ep_sq = board.history[board.getPly()].ep_sq;
        bool stm = board.getTurn() == WHITE;

        unsigned result = tb_probe_wdl(
            w_occ, b_occ, kings, queens, rooks, bishops, knights,  pawns, fifty_move_counter, any_castling, ep_sq == NO_SQUARE ? 0 : ep_sq, stm);

        if (result == TB_LOSS) {
            return VALUE_TB_LOSS;
        }
        if (result == TB_WIN) {
            return VALUE_TB_WIN;
        }
        if (result == TB_DRAW || result == TB_BLESSED_LOSS || result == TB_CURSED_WIN) {
            return VALUE_DRAW;
        }

        // if none of them above happened, act as if the result failed
        return VALUE_NONE;
    }

    Search::Search(const std::string &fen) : board(fen) {
        tt = new TTable(16);
        reset();
    }

    Search::~Search() {
        delete tt;
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
        const bool tt_hit = tt->lookup(tt_entry, hash);
        const Score tt_score = tt_hit ? scoreFromTT(tt_entry.score, ss->ply) : static_cast<Score>(VALUE_NONE);
        Bound tt_bound = tt_entry.bound;

        if (tt_hit
            && !pv_node
            && tt_score != VALUE_NONE
            && tt_bound != NO_BOUND) {
            bool is_exact = tt_bound == EXACT_BOUND;
            bool is_lower_bound = tt_bound == LOWER_BOUND && tt_score >= beta;
            bool is_upper_bound = tt_bound == UPPER_BOUND && tt_score <= alpha;

            if (is_exact || is_lower_bound || is_upper_bound) {
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

        MoveList moves(board, CAPTURE_MOVES);
        move_ordering.sortMoves(board, moves, tt_entry.move, (ss - 1)->current_move, ss->ply);

        Move best_move = NO_MOVE;
        for (Move move: moves) {
            //assert(isCapture(move));

            if (best_score > VALUE_TB_LOSS_IN_MAX_PLY && !in_check) {
                // delta pruning
                PieceType captured = typeOf(board.pieceAt(move.to()));
                if (captured != NO_PIECE_TYPE &&
                    best_score + DELTA_MARGIN + PIECE_VALUES[captured] < alpha
                    && !isPromotion(move)
                    && board.nonPawnMat(stm)) {
                    continue;
                }

                // see pruning
                if (!see(board, move, 0)) {
                    continue;
                }
            }

            nodes++;

            board.makeMove(move, tt, true);
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

        // store in transposition table
        Bound bound = best_score >= beta ? LOWER_BOUND : UPPER_BOUND;
        tt->store(hash, best_move, scoreToTT(best_score, ss->ply), 0, bound);

        assert(best_score > -VALUE_INFINITE && best_score < VALUE_INFINITE);
        return best_score;
    }

    Score Search::abSearch(int depth, Score alpha, Score beta, Node node, Stack *ss) {
        if (time_manager.isLimitReached(depth, nodes)) {
            return beta;
        }

        if (ss->ply >= MAX_PLY) {
            return Eval::evaluate(board);
        }

        pv_table(ss->ply).length = ss->ply;

        const Color stm = board.getTurn();
        const bool in_check = board.inCheck();

        const bool root_node = node == ROOT;
        const bool pv_node = node != NON_PV;

        if (!root_node && board.isDraw()) {
            return VALUE_DRAW;
        }

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

        // selective depth
        if (pv_node && ss->ply > sel_depth) {
            // heighest depth a pv node has reached
            sel_depth = ss->ply;
        }

        // look up in transposition table
        TTEntry tt_entry;
        const U64 hash = board.getHash();
        const bool tt_hit = tt->lookup(tt_entry, hash);
        const Score tt_score = tt_hit ? scoreFromTT(tt_entry.score, ss->ply) : static_cast<Score>(VALUE_NONE);

        const Move excluded_move = ss->excluded_move;

        if (!root_node
            && !pv_node
            && excluded_move == NO_MOVE
            && tt_hit
            && tt_score != VALUE_NONE
            && tt_entry.depth >= depth
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

            // check for a cutoff
            if (alpha >= beta) {
                return tt_score;
            }
        }

        Score max_score = VALUE_INFINITE;
        Score best_score = -VALUE_INFINITE;

        // tablebase probing
        if (use_TB && !root_node) {
            Score tb_score = probeWDL(board);

            if (tb_score != VALUE_NONE) {
                std::cout << "F" << std::endl;
                Bound bound = NO_BOUND;

                switch (tb_score) {
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

                if (bound == EXACT_BOUND || (bound == LOWER_BOUND && tb_score >= beta) || (bound == UPPER_BOUND && tb_score <= alpha)) {
                    tt->store(hash, NO_MOVE, scoreToTT(tb_score, ss->ply), depth + 6, bound);
                    return tb_score;
                }

                if (pv_node) {
                    if (bound == LOWER_BOUND) {
                        best_score = tb_score;
                        alpha = std::max(alpha, best_score);
                    } else {
                        max_score = tb_score;
                    }
                }
            }
        }

        bool is_improving = false;

        if (in_check) {
            ss->static_eval = VALUE_NONE;
        } else {
            ss->static_eval = tt_hit && tt_score != VALUE_NONE ? tt_score : Eval::evaluate(board);
            is_improving = ss->static_eval > (ss - 2)->static_eval;
        }

        // only use pruning/reduction when not in check and root/pv node
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
            if (!pv_node
                && depth <= 3
                && ss->static_eval + RAZOR_MARGIN < alpha) {
                return qSearch(alpha, beta, NON_PV, ss);
            }

            // reverse futility pruning
            int rfp_margin = ss->static_eval - 62 * depth + 71 * is_improving;
            if (!pv_node
                && depth < 7
                && rfp_margin >= beta
                && std::abs(beta) < VALUE_TB_WIN_IN_MAX_PLY) {
                return beta;
            }

            // null move pruning
            if (!pv_node
                && board.nonPawnMat(stm)
                && excluded_move == NO_MOVE
                && (ss - 1)->current_move != NULL_MOVE
                && depth >= 3
                && ss->static_eval >= beta) {
                constexpr int R = 5;

                ss->current_move = NULL_MOVE;

                board.makeNullMove();
                Score score = -abSearch(depth - R, -beta, -beta + 1, NON_PV, ss + 1);
                board.unmakeNullMove();

                if (score >= beta) {
                    // don't return unproven mate scores
                    if (score >= VALUE_TB_WIN_IN_MAX_PLY) {
                        return beta;
                    }

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
        move_ordering.sortMoves(board, moves, tt_entry.move, (ss - 1)->current_move, ss->ply);

        Score score = VALUE_NONE;

        Move best_move = NO_MOVE;

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

            if (!root_node && best_score > VALUE_TB_LOSS_IN_MAX_PLY) {
                if (is_capture) {
                    // see pruning
                    if (depth < 6 && !see(board, move, -100 * depth)) {
                        continue;
                    }
                } else {
                    // late move pruning
                    if (!in_check
                        && !pv_node
                        && !is_promotion
                        && depth <= 5
                        && quiet_count > (4 + depth * depth)) {
                        continue;
                    }

                    // see pruning
                    if (depth < 7 && !see(board, move, -40 * depth)) {
                        continue;
                    }
                }
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
                && tt_entry.depth >= depth - 3) {
                const Score singular_beta = tt_score - 3 * depth;
                const int singular_depth = (depth - 1) / 2;

                ss->excluded_move = move;
                const auto value = abSearch(singular_depth, singular_beta - 1, singular_beta, NON_PV, ss);
                ss->excluded_move = NO_MOVE;

                if (value < singular_beta) {
                    extension = 1;
                } else if (singular_beta >= beta) {
                    return singular_beta;
                }
            }

            // one reply extension
            if (in_check && (ss - 1)->move_count == 1 && ss->move_count == 1) {
                extension += 1;
            }

            const int newDepth = depth - 1 + extension;

            nodes++;
            board.makeMove(move, tt, true);

            ss->current_move = move;

            // late move reduction
            if (depth >= 2 && !in_check && made_moves > 3) {
                int rdepth = REDUCTIONS[depth][made_moves];
                rdepth += is_improving;
                rdepth -= pv_node;
                rdepth -= is_capture;
                rdepth = std::clamp(newDepth - rdepth, 1, newDepth + 1);

                score = -abSearch(rdepth, -alpha - 1, -alpha, NON_PV, ss + 1);

                // if late move reduction failed high, research
                if (score > alpha && rdepth < newDepth) {
                    score = -abSearch(newDepth, -alpha - 1, -alpha, NON_PV, ss + 1);
                }
            } else if (!pv_node || made_moves > 1) {
                // full-depth search if lmr was skipped
                score = -abSearch(newDepth, -alpha - 1, -alpha, NON_PV, ss + 1);
            }

            // principal variation search
            if (pv_node && ((score > alpha && score < beta) || made_moves == 1)) {
                score = -abSearch(newDepth, -beta, -alpha, PV, ss + 1);
            }

            board.unmakeMove(move);

            assert(score > -VALUE_INFINITE && score < VALUE_INFINITE);

            if (score > best_score) {
                best_score = score;

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

        ss->move_count = made_moves;

        // check for mate and stalemate
        if (moves.size() == 0) {
            if (excluded_move != NO_MOVE) {
                best_score = alpha;
            } else {
                best_score = in_check ? -VALUE_MATE + ss->ply : VALUE_DRAW;
            }
        }

        if (pv_node) {
            best_score = std::min(best_score, max_score);
        }

        // store in transposition table
        if (excluded_move == NO_MOVE) {
            Bound bound;
            if (best_score >= beta) {
                bound = LOWER_BOUND;
            } else if (pv_node && best_move != NO_MOVE) {
                bound = EXACT_BOUND;
            } else {
                bound = UPPER_BOUND;
            }

            tt->store(hash, best_move, scoreToTT(best_score, ss->ply), depth, bound);
        }

        assert(best_score > -VALUE_INFINITE && best_score < VALUE_INFINITE);
        return best_score;
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

            if (time_manager.isLimitReached(depth, nodes)) {
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

    SearchResult Search::bestMove(int max_depth) {
        time_manager.start();

        SearchResult search_result;

        Stack stack[MAX_PLY + 4];
        Stack *ss = stack + 2;

        for (int i = 2; i > 0; --i) {
            (ss - i)->ply = i;
            (ss - i)->move_count = 0;
            (ss - i)->current_move = NO_MOVE;
            (ss - i)->static_eval = 0;
            (ss - i)->excluded_move = NO_MOVE;
        }

        for (int i = 0; i <= MAX_PLY + 1; ++i) {
            (ss + i)->ply = i;
            (ss + i)->move_count = 0;
            (ss + i)->current_move = NO_MOVE;
            (ss + i)->static_eval = 0;
            (ss + i)->excluded_move = NO_MOVE;
        }

        move_ordering.clearHistory();
        move_ordering.clearCounters();

        int depth = 1;
        for (; depth <= max_depth; depth++) {
            pv_table.reset();

            sel_depth = 0;

            const Score previous_result = search_result.score;
            const Score result = aspSearch(depth, previous_result, ss);

            if (time_manager.isLimitReached(depth, nodes)) {
                break;
            }

            search_result.best_move = pv_table(0)(0);
            search_result.score = result;

            // info for uci
            std::cout << "info depth " << depth
                    << " seldepth " << static_cast<int>(sel_depth)
                    << " score ";

            if (result >= VALUE_MIN_MATE) {
                std::cout << "mate " << (VALUE_MATE - result) / 2 + ((VALUE_MATE - result) & 1);
            } else if (result <= -VALUE_MIN_MATE) {
                std::cout << "mate " << -((VALUE_MATE + result) / 2) + ((VALUE_MATE + result) & 1);
            } else {
                std::cout << "cp " << result;
            }

            int elapsed_time = time_manager.elapsedTime();

            std::cout << " nodes " << nodes
                    << " nps " << nodes * 1000 / (elapsed_time + 1)
                    << " time " << elapsed_time;

            std::string pv = getPv();
            std::cout << " pv";
            if (pv == "") {
                std::cout << " " << pv_table(0)(0);
            } else {
                std::cout << pv;
            }
            std::cout << std::endl;
        }

        if (depth == 1) {
            search_result.best_move = pv_table(0)(0);
        }

        return search_result;
    }

    void Search::reset() {
        board = Board(STARTING_FEN);
        nodes = 0;
        tt->clear();
        pv_table.reset();
        move_ordering.clearHistory();
        move_ordering.clearCounters();
        move_ordering.clearKillers();
    }

    std::string Search::getPv() {
        std::ostringstream pv;

        for (int i = 0; i < pv_table(0).length; i++) {
            pv << " " << pv_table(0)(i);
        }

        return pv.str();
    }

} // namespace Astra
