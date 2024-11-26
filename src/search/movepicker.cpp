#include "movepicker.h"

namespace Astra
{
    MovePicker::MovePicker(MoveType mt, Board &board, const History &history, const Stack *ss, Move &tt_move)
        : mt(mt), board(board), history(history), ss(ss), ml(board, mt), tt_move(tt_move)
    {
        ml_size = ml.size();
    }

    Move MovePicker::nextMove(bool skip_quiets)
    {
        switch (stage)
        {
        case TT:
            stage = SCORE;
            if (tt_move != NO_MOVE && ml.find(tt_move) != -1)
            {
                ml_tt_move = tt_move;
                return tt_move;
            }
            [[fallthrough]];
        case SCORE:
            stage = GOOD_CAPTURES;
            scoreMoves();
            [[fallthrough]];
        case GOOD_CAPTURES:
            while (idx < ml_size)
            {
                partialInsertionSort(idx);

                if (ml[idx].score < 7'000'000)
                    break; // done with good captures

                Move move = ml[idx];
                idx++;

                // skip tt move since we already returned it
                if (move != ml_tt_move)
                    return move;
            }

            stage = KILLER1;
            [[fallthrough]];
        case KILLER1:
            stage = KILLER2;
            if (!skip_quiets && killer1 != NO_MOVE && killer1 != ml_tt_move)
                return killer1;
            [[fallthrough]];
        case KILLER2:
            stage = COUNTER;
            if (!skip_quiets && killer2 != NO_MOVE && killer2 != ml_tt_move)
                return killer2;
            [[fallthrough]];
        case COUNTER:
            stage = REST;
            if (!skip_quiets && counter != NO_MOVE && counter != killer1 && counter != killer2 && counter != ml_tt_move)
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

            // don't accidentally set tt move as a killer or counter
            if (ml[i] == ml_tt_move)  
            {
                ml[i].score = 10'000'000; 
            }
            else if (captured != NO_PIECE_TYPE)
                ml[i].score = 7'000'000 * board.see(ml[i], 0) + PIECE_VALUES[captured] + history.getCHScore(board, ml[i]);
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
            else if (history.getCounterMove((ss - 1)->curr_move) == ml[i])
            {
                ml[i].score = 4'000'000;
                counter = ml[i];
            }
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
