#include "history.h"
#include "search.h"

namespace Astra
{
    History::History()
    {
        clear();
        history_bonus = 0;
    }

    void History::clear()
    {
        for (auto &i : history)
            for (auto &j : i)
                for (int &k : j)
                    k = 0;

        for (auto &counter : counters)
            for (auto &j : counter)
                j = NO_MOVE;

        for (int i = 0; i < MAX_PLY; ++i)
        {
            killer1[i] = NO_MOVE;
            killer2[i] = NO_MOVE;
        }
    }

    void History::update(Board &board, Move &move, Move *quiet_moves, Stack *ss, int quiet_count, int depth)
    {
        int bonus = historyBonus(depth);

        Move prev_move = (ss - 1)->current_move;
        if (prev_move != NO_MOVE)
            counters[prev_move.from()][prev_move.to()] = move;

        if (!isCapture(move))
        {
            killer2[ss->ply] = killer1[ss->ply];
            killer1[ss->ply] = move;

            Color c = board.getTurn();
            if (depth > 1)
                updateBonus(move, c, bonus);

            // history maluses
            for (int i = 0; i < quiet_count; i++)
                updateBonus(quiet_moves[i], c, -bonus);
        }
    }

    // private member

    void History::updateBonus(Move &move, Color c, int bonus)
    {
        Square from = move.from();
        Square to = move.to();
        history[c][from][to] += bonus - history[c][from][to] * std::abs(bonus) / 16384;
    }

    int History::historyBonus(int depth)
    {
        return std::min(1600, depth * history_bonus);
    }

} // namespace Astra
