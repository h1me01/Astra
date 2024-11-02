#ifndef MOVEORDERING_H
#define MOVEORDERING_H

#include "../chess/board.h"
#include "../chess/movegen.h"
#include "../search/history.h"

namespace Astra
{
    constexpr int mvvlva_table2[7][7] = {
        {5, 4, 3, 2, 1, 0, 0},
        {15, 14, 13, 12, 11, 10, 0},
        {25, 24, 23, 22, 21, 20, 0},
        {35, 34, 33, 32, 31, 30, 0},
        {45, 44, 43, 42, 41, 40, 0},
        {55, 54, 53, 52, 51, 50, 0}};

    inline int mvvlva2(const Board &board, const Move &move)
    {
        const int attacker = typeOf(board.pieceAt(move.from()));
        const int victim = typeOf(board.pieceAt(move.to()));
        return mvvlva_table2[victim][attacker];
    }

    inline void sortMoves(History& history, Board &board, MoveList &moves, Move &tt_move, Move &prev_move, int ply)
    {
        std::vector<int> scores(moves.size(), 0);

        uint8_t move_count = 0;
        for (Move move : moves)
        {
            if (move == tt_move)
                scores[move_count] =  10'000'000;
            else if (isCapture(move))
            {
                const int mvvlva_score = mvvlva2(board, move);
                scores[move_count] = board.see(move, 0) ? 7'000'000 + mvvlva_score : mvvlva_score;
            }
            else if (move == history.getKiller1(ply))
                scores[move_count] = 6'000'000;
            else if (move == history.getKiller2(ply))
                scores[move_count] = 5'000'000;
            else if (history.getCounterMove(prev_move) == move)
                scores[move_count] = 4'000'000;
            else
                scores[move_count] = history.getHHScore(board.getTurn(), move);

            move_count++;
        }

        // bubble sort moves based on their scores
        int n = moves.size();
        bool swapped;

        do
        {
            swapped = false;
            for (int i = 0; i < n - 1; ++i)
            {
                if (scores[i] < scores[i + 1])
                {
                    int temp_score = scores[i];
                    scores[i] = scores[i + 1];
                    scores[i + 1] = temp_score;

                    Move temp_move = moves[i];
                    moves[i] = moves[i + 1];
                    moves[i + 1] = temp_move;

                    swapped = true;
                }
            }

            n--;
        } while (swapped);
    }

} // namespace Astra

#endif //MOVEORDERING_H