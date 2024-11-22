#include "movepicker.h"

namespace Astra
{
    MovePicker::MovePicker(MoveType mt, Board &board, const History &history, const Stack *ss, Move &tt_move)
        : mt(mt), board(board), history(history), ss(ss), ml(board, mt), tt_move(tt_move)
    {
        ml_size = ml.size();
    }

    Move MovePicker::nextMove()
    {
        switch (stage)
        {
        case TT:
            stage++;
            if (tt_move != NO_MOVE && ml.find(tt_move) != -1)
                return tt_move;
            [[fallthrough]];
        case EVAL:
            stage++;
            evaluateMoves();
            [[fallthrough]];
        case GOOD_CAPTURES:
        {
            while (idx < ml_size)
            {
                partialInsertionSort(idx);

                Move move = ml[idx];
                idx++;

                // skip tt move since we already returned it
                if (move == tt_move)
                    continue;

                return move;
            }

            stage++;
            [[fallthrough]];
        }
        case KILLER1:
            stage++;
            if (killer1 != NO_MOVE) 
                return killer1;
            [[fallthrough]];
        case KILLER2:
            stage++;
            if (killer2 != NO_MOVE) 
                return killer2;
            [[fallthrough]];
        case COUNTER:
            stage++;
            if (counter != NO_MOVE) 
                return counter;
            [[fallthrough]];
        case REST:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);

                Move move = ml[idx];
                idx++;

                // skip tt move since we already returned it
                if (move == tt_move || move == killer1 || move == killer2 || move == counter)
                    continue;

                return move;
            }

            // no more moves left
            return NO_MOVE;
        default:
            assert(false); // we should never reach this
            return NO_MOVE;
        }
    }

    // private member

    void MovePicker::evaluateMoves()
    {
        for (int i = 0; i < ml_size; i++)
        {
            if (ml[i] == tt_move)
                ml[i].score = 10'000'000;
            else if (board.isCapture(ml[i])) 
            {
                PieceType captured = typeOf(board.pieceAt(ml[i].to()));
                int ch_score = history.getCHScore(board, ml[i]);
                ml[i].score = 7'000'000 * board.see(ml[i], 0) + PIECE_VALUES[captured] + ch_score + 1000 * isPromotion(ml[i]); 
            }
            else if (ml[i] == history.getKiller1(ss->ply)) 
            {
                ml[i].score = 6'000'000;
                killer1 = ml[i];
            }
            else if (ml[i] == history.getKiller2(ss->ply)) 
            {
                ml[i].score = 5'000'000; 
                killer2 = ml[i];
            }
            else if (history.getCounterMove((ss - 1)->current_move) == ml[i]) 
            {
                ml[i].score = 4'000'000;
                counter = ml[i];
            }
            else
            {
                assert(mt == ALL_MOVES); // qsearch should never reach this
                ml[i].score = 2 * history.getQHScore(board.getTurn(), ml[i]);
                ml[i].score += 2 * history.getContHScore(board, ml[i], (ss - 1)->current_move);
                ml[i].score += 2 * history.getContHScore(board, ml[i], (ss - 2)->current_move);
                ml[i].score += history.getContHScore(board, ml[i], (ss - 4)->current_move);
                ml[i].score += history.getContHScore(board, ml[i], (ss - 6)->current_move);
            }
        }
    }

    void MovePicker::partialInsertionSort(int start)
    {
        int best_idx = start;

        for (int i = 1 + start; i < ml_size; i++)
            if (ml[i].score > ml[best_idx].score)
                best_idx = i;

        // swap
        Move temp = ml[best_idx];
        int temp_score = ml[best_idx].score;

        ml[best_idx] = ml[start];
        ml[best_idx].score = ml[start].score;

        ml[start] = temp;
        ml[start].score = temp_score;
    }

} // namespace Astra
