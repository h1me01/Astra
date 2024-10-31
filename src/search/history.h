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

        void clear();
        void update(Board &board, Move &move, Move *quiet_moves, Stack *ss, int quiet_count, int depth);

        void setHistoryBonus(int bonus) { history_bonus = bonus; }

        int getHistoryScore(Color c, Move &move) const
        {
            return history[c][move.from()][move.to()];
        }

        Move getCounterMove(Move move) const
        {
            return counters[move.from()][move.to()];
        }

        Move getKiller1(int ply) const { return killer1[ply]; }
        Move getKiller2(int ply) const { return killer2[ply]; }

    private:
        int history_bonus;

        Move killer1[MAX_PLY];
        Move killer2[MAX_PLY];
        Move counters[NUM_SQUARES][NUM_SQUARES];

        int history[NUM_COLORS][NUM_SQUARES][NUM_SQUARES]{};

        void updateBonus(Move &move, Color c, int bonus);

        int historyBonus(int depth);
    };

} // namespace Astra

#endif // HISTORY_H