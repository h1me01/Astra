#include "movepicker.h"

namespace Astra
{
    MovePicker::MovePicker(SearchType st, Board &board, const History &history, const Stack *ss, Move &tt_move, bool in_check)
        : st(st), board(board), history(history), ss(ss), ml(board), tt_move(tt_move), in_check(in_check)
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

    Move MovePicker::nextMove(bool skip_quiets)
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

                if (move.score < 1e7)
                    break; // done with good captures

                idx++;

                // skip tt move since we already returned it
                if (move != ml_tt_move)
                    return move;
            }

            if (st == PC_SEARCH)
                return NO_MOVE;
            
            stage = KILLER1;

            [[fallthrough]];
        case KILLER1:
            stage = KILLER2;
            if (!skip_quiets && killer1 != NO_MOVE)
                return killer1;
            [[fallthrough]];
        case KILLER2:
            stage = COUNTER;
            if (!skip_quiets && killer2 != NO_MOVE)
                return killer2;
            [[fallthrough]];
        case COUNTER:
            stage = REST;
            if (!skip_quiets && counter != NO_MOVE && counter != killer1 && counter != killer2)
                return counter;
            [[fallthrough]];
        case REST:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;

                if (!board.isCapture(move) && skip_quiets)
                    continue;

                // skip tt move, killer1, killer2, and counter
                if (move != ml_tt_move && move != killer1 && move != killer2 && move != counter)
                    return move;
            }

            return NO_MOVE;
        case Q_MOVES:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;
                
                if (board.isCapture(move) || board.givesCheck(move))
                    return move;
            }

            return NO_MOVE; // no more moves

            [[fallthrough]];
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

            PieceType captured = ml[i].flag() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(ml[i].to()));

            if (st == Q_SEARCH && !in_check)
            {
                if (captured != NO_PIECE_TYPE)
                    ml[i].score = 1e7 * board.see(ml[i], 0) + PIECE_VALUES[captured] + history.getCHScore(board, ml[i]);

                continue; // don't score quiet moves
            }

            if (ml[i] == tt_move)
                ml_tt_move = tt_move; 
            // add 1e7 when in check so we play the captures before quiet moves
            else if (captured != NO_PIECE_TYPE) 
                ml[i].score = in_check * 1e7 + 1e7 * board.see(ml[i], 0) + PIECE_VALUES[captured] + history.getCHScore(board, ml[i]);
            else if (ml[i] == history.getKiller1(ss->ply) && !in_check)
                killer1 = ml[i];
            else if (ml[i] == history.getKiller2(ss->ply) && !in_check)
                killer2 = ml[i];
            else if (history.getCounterMove((ss - 1)->curr_move) == ml[i] && !in_check)
                counter = ml[i];
            else
            {
                ml[i].score = 2 * history.getQHScore(board.getTurn(), ml[i]);
                ml[i].score += 2 * history.getContHScore(board, ml[i], (ss - 1)->curr_move);
                ml[i].score += 2 * history.getContHScore(board, ml[i], (ss - 2)->curr_move);
                ml[i].score += history.getContHScore(board, ml[i], (ss - 4)->curr_move);
                ml[i].score += history.getContHScore(board, ml[i], (ss - 6)->curr_move);
            }
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
