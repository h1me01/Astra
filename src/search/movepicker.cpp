#include "movepicker.h"
#include <unordered_set>

namespace Astra
{
    void partialInsertionSort(MoveList& ml, int idx)
    {
        int best_idx = idx;

        for (int i = 1 + idx; i < ml.size(); i++)
            if (ml[i].score > ml[best_idx].score)
                best_idx = i;

        std::swap(ml[idx], ml[best_idx]);
    }

    // MovePicker class

    MovePicker::MovePicker(SearchType st, const Board &board, const History &history, const Stack *ss, Move &tt_move, bool gen_checkers)
        : st(st), board(board), history(history), ss(ss), tt_move(tt_move), gen_checkers(gen_checkers)
    {
        if (st == PC_SEARCH)
            stage = PC_GEN_NOISY;
        else if (st == Q_SEARCH)
            stage = board.inCheck() ? Q_IN_CHECK_TT : Q_GEN_NOISY;
        else 
            stage = TT;

        Move prev_move = (ss - 1)->curr_move;
        if (prev_move != NO_MOVE)
            counter = history.getCounterMove(prev_move);
        else
            counter = NO_MOVE;

        killer1 = ss->killer1;
        killer2 = ss->killer2;
    }

    Move MovePicker::nextMove()
    {
        switch (stage)
        {
        case TT:
            stage = GEN_NOISY;
            if (board.isPseudoLegal(tt_move))
                return tt_move;
            [[fallthrough]];
        case GEN_NOISY:
            idx = 0;
            stage = PLAY_GOOD_NOISY;

            ml_main.gen<NOISY>(board);
            scoreNoisyMoves();
            
            [[fallthrough]];
        case PLAY_GOOD_NOISY:
            while (idx < ml_main.size())
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;

                // skip tt move 
                if (move == tt_move)
                    continue;

                if (!board.see(move, -move.score / 16))
                {
                    // add to bad noisy
                    ml_bad_noisy.add(move);
                    continue;
                }

                return move;
            }

            stage = PLAY_KILLER1;
            [[fallthrough]];
        case PLAY_KILLER1:
            stage = PLAY_KILLER2;
            if (!skip_quiets && killer1 != tt_move && board.isPseudoLegal(killer1))
                return killer1;
            [[fallthrough]];
        case PLAY_KILLER2:
            stage = PLAY_COUNTER;
            if (!skip_quiets && killer2 != tt_move && board.isPseudoLegal(killer2))
                return killer2;
            [[fallthrough]];
        case PLAY_COUNTER:
            stage = GEN_QUIETS;
            if (!skip_quiets && counter != tt_move && counter != killer1 && counter != killer2 && board.isPseudoLegal(counter))
                return counter;
            [[fallthrough]];
        case GEN_QUIETS:
            idx = 0;
            stage = PLAY_QUIETS;

            if (!skip_quiets)
            {
                ml_main.gen<QUIETS>(board);
                scoreQuietMoves();
            }

            [[fallthrough]];
        case PLAY_QUIETS:
            while (idx < ml_main.size() && !skip_quiets)
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;
                
                // skip tt move, killers, and counter
                if (move == tt_move || move == killer1 || move == killer2 || move == counter)
                    continue;
                
                return move;
            }

            idx = 0;
            stage = PLAY_BAD_NOISY;
            
            [[fallthrough]];
        case PLAY_BAD_NOISY:
            while (idx < ml_bad_noisy.size())
            {
                partialInsertionSort(ml_bad_noisy, idx);
                Move move = ml_bad_noisy[idx];
                idx++;

                // skip tt move
                if (move == tt_move)
                    continue;

                assert(isCap(move) || move.type() == PQ_QUEEN);
                return move;
            }

            return NO_MOVE; // no more moves
        case PC_GEN_NOISY:
            idx = 0;
            stage = PC_PLAY_GOOD_NOISY;
            
            ml_main.gen<NOISY>(board);
            scoreNoisyMoves();
            
            [[fallthrough]];
        case PC_PLAY_GOOD_NOISY:
            while (idx < ml_main.size())
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;

                if (!board.see(move, see_cutoff))
                    continue;

                return move;
            }

            return NO_MOVE; // no more moves
        case Q_GEN_NOISY:
            idx = 0;
            stage = Q_PLAY_NOISY;
            
            ml_main.gen<NOISY>(board);
            scoreNoisyMoves();
            
            [[fallthrough]];
        case Q_PLAY_NOISY:
            while (idx < ml_main.size())
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;
                return move;
            }

            if (!gen_checkers)
                return NO_MOVE; // no more moves

            stage = Q_GEN_QUIET_CHECKERS;
            
            [[fallthrough]];
        case Q_GEN_QUIET_CHECKERS:
            idx = 0;
            stage = Q_PLAY_QUIET_CHECKERS;
            ml_main.gen<QUIET_CHECKERS>(board);
            [[fallthrough]];
        case Q_PLAY_QUIET_CHECKERS:
            while (idx < ml_main.size())
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;
                return move;
            }

            return NO_MOVE; // no more moves
        case Q_IN_CHECK_TT:
            stage = Q_IN_CHECK_GEN_NOISY;
            if (board.isPseudoLegal(tt_move))
                return tt_move;
            [[fallthrough]];
        case Q_IN_CHECK_GEN_NOISY:
            idx = 0;
            stage = Q_IN_CHECK_PLAY_NOISY;
            
            ml_main.gen<NOISY>(board);
            scoreNoisyMoves();
            
            [[fallthrough]];
        case Q_IN_CHECK_PLAY_NOISY:
            while (idx < ml_main.size())
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;

                // skip tt move
                if (move == tt_move)
                    continue;

                return move;
            }

            if (skip_quiets)
                return NO_MOVE; // no more moves

            stage = Q_IN_CHECK_GEN_QUIETS;
            
            [[fallthrough]];
        case Q_IN_CHECK_GEN_QUIETS:
            idx = 0;
            stage = Q_IN_CHECK_PLAY_QUIETS;
            
            if (!skip_quiets)
            {
                ml_main.gen<QUIETS>(board);
                scoreQuietMoves();
            }

            [[fallthrough]];
        case Q_IN_CHECK_PLAY_QUIETS:
            while (idx < ml_main.size() && !skip_quiets)
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;

                // skip tt move
                if (move == tt_move)
                    continue;

                return move;
            }

            return NO_MOVE; // no more moves
        default:
            assert(false); // we should never reach this
            return NO_MOVE;
        }
    }

    // private member

    void MovePicker::scoreQuietMoves()
    {
        for (int i = 0; i < ml_main.size(); i++)
        {
            assert(!isCap(ml_main[i]));
           
            const Piece pc = board.pieceAt(ml_main[i].from());
            const PieceType pt = typeOf(pc);
            const Square from = ml_main[i].from();
            const Square to = ml_main[i].to();

            ml_main[i].score = history.getHistoryHeuristic(board.getTurn(), ml_main[i]);
            ml_main[i].score += (ss - 1)->cont_history[pc][to];
            ml_main[i].score += (ss - 2)->cont_history[pc][to];
            ml_main[i].score += (ss - 4)->cont_history[pc][to] / 2;
            ml_main[i].score += (ss - 6)->cont_history[pc][to] / 4;

            if (pt != PAWN && pt != KING)
            {
                U64 danger;
                if (pt == QUEEN)
                    danger = board.getThreats(ROOK);
                else if (pt == ROOK)
                    danger = board.getThreats(BISHOP) | board.getThreats(KNIGHT);
                else
                    danger = board.getThreats(PAWN);

                if (danger & SQUARE_BB[from]) 
                    ml_main[i].score += 16384 + 16384 * (pt == QUEEN);
                else if (danger & SQUARE_BB[to])
                    ml_main[i].score -= 16384 + 16384 * (pt == QUEEN);
            }
        }
    }

    void MovePicker::scoreNoisyMoves()
    {
        for (int i = 0; i < ml_main.size(); i++)
        {
            assert(isCap(ml_main[i]) || ml_main[i].type() == PQ_QUEEN);
            
            PieceType captured = ml_main[i].type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(ml_main[i].to()));
            bool is_cap = captured != NO_PIECE_TYPE; // quiet queen prom is not a capture

            ml_main[i].score = PIECE_VALUES[captured] + isProm(ml_main[i]) * 8192 + (is_cap ? history.getCaptureHistory(board, ml_main[i]) : 0);
        }
    }

} // namespace Astra
