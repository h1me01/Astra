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

    int mvvlva(const Board &board, Move &move)
    {
        assert(board.isCapture(move));
        const PieceType attacker = typeOf(board.pieceAt(move.from()));
        const PieceType victim = typeOf(board.pieceAt(move.to()));
        return mvvlva_table[victim][attacker];
    }

    // movepicker class
    MovePicker::MovePicker(MoveType mt, Board &board, const History &history, const Stack *ss, Move &tt_move)
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
        case REST:
        {
            while (idx < ml.size())
            {
                partialInsertionSort(idx);

                Move move = ml[idx];
                idx++;

                // skip tt move since we already returned it
                if (move == tt_move)
                    continue;

                return move;
            }

            // no more moves left
            return NO_MOVE;
        }
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
                ml[i].score = 10'000'000;
            else if (board.isCapture(ml[i]))
                ml[i].score = (board.see(ml[i], 0) ? 7'000'000 : 0) + mvvlva(board, ml[i]);
            else if (ml[i] == history.getKiller1(ss->ply))
                ml[i].score = 6'000'000;
            else if (ml[i] == history.getKiller2(ss->ply))
                ml[i].score = 5'000'000;
            else if (history.getCounterMove((ss - 1)->current_move) == ml[i])
                ml[i].score = 4'000'000;
            else
            {
                assert(mt == ALL_MOVES); // qsearch should never reach this
                ml[i].score = history.getHHScore(board.getTurn(), ml[i]);
                ml[i].score += 2 * history.getCHScore(board, ml[i], (ss - 1)->current_move);
                ml[i].score += history.getCHScore(board, ml[i], (ss - 2)->current_move);
            }
        }
    }

    void MovePicker::partialInsertionSort(int start)
    {
        int best_idx = start;

        for (int i = 1 + start; i < ml.size(); i++)
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
