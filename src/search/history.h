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
        void update(Board &board, Move &move, Stack *ss, Move *q_moves, int qc, Move* c_moves, int cc, int depth);

        int getQHScore(Color c, Move &move) const;
        int getCHScore(Board &board, Move &move) const;
        int getContHScore(Board &board, Move &move, const Move &prev_move) const;

        Move getCounterMove(Move move) const;
        Move getKiller1(int ply) const { return killer1[ply]; }
        Move getKiller2(int ply) const { return killer2[ply]; }

    private:
        Move killer1[MAX_PLY];
        Move killer2[MAX_PLY];
        Move counters[NUM_SQUARES][NUM_SQUARES];

        static int16_t quiet_history[NUM_COLORS][NUM_SQUARES][NUM_SQUARES];
        static int16_t cont_history[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECES + 1][NUM_SQUARES];
        static int16_t capt_history[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECE_TYPES];

        void updateHH(Move &move, Color c, int bonus);
        void updateCH(Board& board, Move &move, int bonus);
        void updateContH(Board &board, Move &move, Stack *ss, int bonus);
    };

} // namespace Astra

#endif // HISTORY_H