#ifndef HISTORY_H
#define HISTORY_H

#include "../chess/board.h"

namespace Astra
{
    struct Stack;

    int historyBonus(int depth);

    class History
    {
    public:
        void update(Board &board, Move &move, Stack *ss, Move *q_moves, int qc, Move* c_moves, int cc, int depth);

        int getQHScore(Color c, Move &move) const;
        int getCHScore(const Board &board, Move &move) const;

        Move getCounterMove(Move move) const;

        void updateQH(Move &move, Color c, int bonus);
        void updateCH(Board& board, Move &move, int bonus);
        void updateContH(Board &board, Move &move, Stack *ss, int bonus);

    private:
        Move counters[NUM_SQUARES][NUM_SQUARES];

        int16_t quiet_history[NUM_COLORS][NUM_SQUARES][NUM_SQUARES] {};
        int16_t capt_history[NUM_PIECES][NUM_SQUARES][NUM_PIECE_TYPES] {};
    };

} // namespace Astra

#endif // HISTORY_H