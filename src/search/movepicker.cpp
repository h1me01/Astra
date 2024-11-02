#include "movepicker.h"

namespace Astra
{
    // most valuable victim, least valuable attacker
    constexpr int mvvlva_table[NUM_PIECE_TYPES][NUM_PIECE_TYPES] = {
        {55, 54, 53, 52, 51, 50},
        {105, 104, 103, 102, 101, 100},
        {205, 204, 203, 202, 201, 200},
        {305, 304, 303, 302, 301, 300},
        {405, 404, 403, 402, 401, 400},
        {505, 504, 503, 502, 501, 500}};

    int mvvlva(const Board &board, Move& move)
    {
        assert(isCapture(move));
        const PieceType attacker = typeOf(board.pieceAt(move.from()));
        const PieceType victim = typeOf(board.pieceAt(move.to()));
        return mvvlva_table[victim][attacker];
    }

    // movepicker class
    MovePicker::MovePicker(MoveType mt, Board &board, const History &history, const Stack *ss, Move& tt_move)
        : mt(mt), board(board), history(history), ss(ss), ml(board, mt), tt_move(tt_move)
    {
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
            while (idx < ml.size())
            {
                int best_idx = partialInsertionSort(idx);

                // if score is less than capture score, then it must be a quiet move or bad capture
                // so we can break out of the loop
                if (ml[best_idx].score < CAPTURE_SCORE)
                    break;

                swapMoves(best_idx, idx);
                Move move = ml[idx];
                idx++;

                // skip tt move since we already returned it
                if (move == tt_move)
                    continue;

                return move;
            }

            // qsearch ends here
            if (mt == CAPTURE_MOVES)
                return NO_MOVE;

            stage++;
            [[fallthrough]];
        }
        case KILLER_1:
            stage++;

            if (killer1 != NO_MOVE)
                return killer1;

            [[fallthrough]];
        case KILLER_2:
            stage++;

            if (killer2 != NO_MOVE)
                return killer2;

            [[fallthrough]];
        case COUNTER:
            stage++;

            if (counter != NO_MOVE)
                return counter;

            [[fallthrough]];
        case BAD:
            while (idx < ml.size())
            {
                int best_idx = partialInsertionSort(idx);

                swapMoves(best_idx, idx);
                Move move = ml[idx];
                idx++;

                // skip tt move, killers and counter since we already returned them
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
        for (int i = 0; i < ml.size(); i++)
        {
            if (ml[i] == tt_move)
            {
                ml[i].score = TT_SCORE;
                continue;
            }

            // in qsearch we don't want to reach BAD stage since we only return captures
            if (mt == CAPTURE_MOVES)
            {
                ml[i].score = CAPTURE_SCORE + mvvlva(board, ml[i]);
                continue;
            }

            if (isCapture(ml[i]))
            {
                const int mvvlva_score = mvvlva(board, ml[i]);
                ml[i].score = board.see(ml[i], 0) ? CAPTURE_SCORE + mvvlva_score : mvvlva_score;
            }
            else if (ml[i] == history.getKiller1(ss->ply))
            {
                ml[i].score = KILLER1_SCORE;
                killer1 = ml[i];
            }
            else if (ml[i] == history.getKiller2(ss->ply))
            {
                ml[i].score = KILLER2_SCORE;
                killer2 = ml[i];
            }
            else if (history.getCounterMove((ss - 1)->current_move) == ml[i])
            {
                ml[i].score = COUNTER_SCORE;
                counter = ml[i];
            }
            else
            {
                assert(mt == ALL_MOVES); // qsearch should never reach this
                ml[i].score = history.getHHScore(board.getTurn(), ml[i]);
                // ml[i].score += 2 * history.getCHScore(board, ml[i], (ss - 1)->current_move);
                // ml[i].score += history.getCHScore(board, ml[i], (ss - 2)->current_move);
            }
        }
    }

    int MovePicker::partialInsertionSort(int start)
    {
        int best_idx = start;

        for (int i = 1 + start; i < ml.size(); i++)
            if (ml[i].score > ml[best_idx].score)
                best_idx = i;

        return best_idx;
    }

    void MovePicker::swapMoves(int i, int j)
    {
        Move temp = ml[i];
        int temp_score = ml[i].score;

        ml[i] = ml[j];
        ml[i].score = ml[j].score;

        ml[j] = temp;
        ml[j].score = temp_score;
    }

} // namespace Astra
