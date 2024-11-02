#ifndef HISTORY_H
#define HISTORY_H

#include "../chess/board.h"

namespace Astra
{
    struct Stack;

    class History
    {
    public:
        History();

        void init(int max_bonus, int bonus_mult);

        void clear();
        void update(Board &board, Move &move, Move *quiet_moves, Stack *ss, int quiet_count, int depth);

        int getHHScore(Color c, Move &move) const
        {
            return history[c][move.from()][move.to()];
        }

        int getCHScore(Board& board, Move &move, const Move &prev_move) const
        {
            Piece p = board.pieceAt(move.from());
            Piece prev_p = board.pieceAt(prev_move.from());
            //return cont_history[prev_p][prev_move.to()][p][move.to()];
        }

        Move getCounterMove(Move move) const
        {
            return counters[move.from()][move.to()];
        }

        Move getKiller1(int ply) const { return killer1[ply]; }
        Move getKiller2(int ply) const { return killer2[ply]; }

    private:
        int history_bonus;
        int max_history_bonus;

        Move killer1[MAX_PLY];
        Move killer2[MAX_PLY];
        Move counters[NUM_SQUARES][NUM_SQUARES];

        int16_t history[NUM_COLORS][NUM_SQUARES][NUM_SQUARES]{};
        //int16_t cont_history[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECES + 1][NUM_SQUARES]{};

        void updateHH(Move &move, Color c, int bonus);
        void updateCH(Board &board, Move &move, Stack *ss, int bonus);
    };

} // namespace Astra

#endif // HISTORY_H