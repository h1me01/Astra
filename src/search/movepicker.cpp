#include "movepicker.h"

namespace Astra
{
    MovePicker::MovePicker(SearchType st, Board &board, History &history, Stack *ss, Move &tt_move, bool in_check, bool gen_checks)
        : st(st), board(board), history(history), ss(ss), ml(board), tt_move(tt_move), in_check(in_check), gen_checks(gen_checks)
    {
        if (st == PC_SEARCH)
            stage = GOOD_CAPTURES;
        else if (st == Q_SEARCH) 
        {
            if (in_check)
                stage = Q_IN_CHECK_TT_MOVE;
            else
                stage = Q_MOVES;
        }

        ml_size = ml.size();
        scoreMoves();
    }

    Move MovePicker::nextMove()
    {
        switch (stage)
        {
        case TT:
            stage = GOOD_CAPTURES;
            if (ml_tt_move != NO_MOVE)
                return tt_move;
            [[fallthrough]];
        case GOOD_CAPTURES:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;

                // skip tt move since we already returned it
                if (move == ml_tt_move)
                    continue;

                if (move.score < 1e7) 
                {
                    idx--;
                    break; // done with good captures
                }

                assert(board.isCapture(move));
                return move;
            }

            if (st == PC_SEARCH)
                return NO_MOVE;
            
            stage = KILLER1;

            [[fallthrough]];
        case KILLER1:
            stage = KILLER2;
            if (killer1 != NO_MOVE && killer1 != ml_tt_move)
                return killer1;
            [[fallthrough]];
        case KILLER2:
            stage = COUNTER;
            if (killer2 != NO_MOVE && killer2 != ml_tt_move)
                return killer2;
            [[fallthrough]];
        case COUNTER:
            stage = QUIET_AND_BAD_CAP;
            if (counter != NO_MOVE && counter != ml_tt_move && counter != killer1 && counter != killer2)
                return counter;
            [[fallthrough]];
        case QUIET_AND_BAD_CAP:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;

                // skip tt move, killers, and counter
                if (move != ml_tt_move && move != killer1 && move != killer2 && move != counter)
                    return move;
            }

            assert(idx == ml_size);
            return NO_MOVE;
        case Q_MOVES:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;

                if (move.score <= 0)
                    break; 

                assert(board.isCapture(move) || board.givesCheck(move));
                return move;
            }

            return NO_MOVE; // no more moves
        case Q_IN_CHECK_TT_MOVE:
            stage = Q_IN_CHECK_REST;
            if (ml_tt_move != NO_MOVE)
                return tt_move;
            [[fallthrough]];
        case Q_IN_CHECK_REST:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;

                if (move != ml_tt_move)
                    return move;
            }

            return NO_MOVE; // no more moves
        default:
            assert(false); // we should never reach this
            return NO_MOVE;
        }
    }

    // private member

    void MovePicker::scoreMoves()
    {
        for (int i = 0; i < ml_size; i++)
        {
            ml[i].score = 0;

            PieceType captured = ml[i].type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(ml[i].to()));

            if (st == Q_SEARCH && !in_check)
            {
                if (captured != NO_PIECE_TYPE) 
                {
                    ml[i].score = 1e7 + 1e7 * board.see(ml[i], 0) + PIECE_VALUES[captured] + history.getCHScore(board, ml[i]);
                    ml[i].score += (ml[i] == tt_move) * 1e8;
                    assert(ml[i].score > 1e6);
                }
                else if(board.givesCheck(ml[i]) && gen_checks)  
                    ml[i].score = 1e6; 
                
                continue; // don't score non-checker quiet moves
            }

            if (ml[i] == tt_move && st != PC_SEARCH)
                ml_tt_move = tt_move; 
            // add 1e8 in qsearch when in check so we play captures before quiet moves
            else if (captured != NO_PIECE_TYPE) 
                ml[i].score = in_check * 1e8 + 1e8 * board.see(ml[i], 0) + PIECE_VALUES[captured] + history.getCHScore(board, ml[i]);
            else if (ml[i] == ss->killer1 && !in_check)
                killer1 = ml[i];
            else if (ml[i] == ss->killer2 && !in_check)
                killer2 = ml[i];
            else if (history.getCounterMove((ss - 1)->curr_move) == ml[i] && !in_check)
                counter = ml[i];
            else
            {
                ml[i].score = 2 * history.getQHScore(board.getTurn(), ml[i]);
                
                Piece pc = board.pieceAt(ml[i].from());
                Square to = ml[i].to();
                ml[i].score += 2 * (ss - 1)->cont_history[pc][to];
                ml[i].score += 2 * (ss - 2)->cont_history[pc][to];
                ml[i].score += (ss - 4)->cont_history[pc][to];
                ml[i].score += (ss - 6)->cont_history[pc][to];
                assert(ml[i].score < 1e7);
            }

            assert(ml[i].score > INT_MIN && ml[i].score < INT_MAX);
        }      
    }

    void MovePicker::partialInsertionSort(int current_idx)
    {
        int best_idx = current_idx;

        for (int i = 1 + current_idx; i < ml_size; i++)
            if (ml[i].score > ml[best_idx].score)
                best_idx = i;

        std::swap(ml[current_idx], ml[best_idx]);
    }

} // namespace Astra
