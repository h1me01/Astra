#include "history.h"
#include "search.h"

namespace Astra
{
    History::History()
    {
        clear();
    }

    void History::init(int max_bonus, int bonus_mult)
    {
        max_history_bonus = max_bonus;
        history_bonus = bonus_mult;
    }

    void History::clear()
    {
        for (auto &i : history)
            for (auto &j : i)
                for (int16_t &k : j)
                    k = 0;
        
        for (auto &i : cont_history)
            for (auto &j : i)
                for (auto &k : j)
                    for (auto &l : k)
                        l = 0;

        for (auto &counter : counters)
            for (auto &j : counter)
                j = NO_MOVE;

        for (int i = 0; i < MAX_PLY; i++)
        {
            killer1[i] = NO_MOVE;
            killer2[i] = NO_MOVE;
        }
    }

    void History::update(Board &board, Move &move, Move *quiet_moves, Stack *ss, int quiet_count, int depth)
    {
        int hh_bonus = std::min(max_history_bonus, depth * history_bonus);
        int ch_bonus = std::min(1500, 4 * depth * depth * depth);

        Move prev_move = (ss - 1)->current_move;
        if (prev_move != NO_MOVE)
            counters[prev_move.from()][prev_move.to()] = move;

        if (!isCapture(move))
        {
            killer2[ss->ply] = killer1[ss->ply];
            killer1[ss->ply] = move;

            Color c = board.getTurn();
            if (depth > 1)
                updateHH(move, c, hh_bonus);

            updateCH(board, move, ss, ch_bonus);

            // history maluses
            for (int i = 0; i < quiet_count; i++)
            {
                Move quiet = quiet_moves[i];
                updateHH(quiet, c, -hh_bonus);
                updateCH(board, quiet, ss, -ch_bonus);
            }
        }
    }

    // private member

    void History::updateHH(Move &move, Color c, int bonus)
    {
        int16_t &score = history[c][move.from()][move.to()];
        score += bonus - score * std::abs(bonus) / 16384;
    }

    void History::updateCH(Board &board, Move &move, Stack *ss, int bonus)
    {
        Piece curr_piece = board.pieceAt(move.from());
        assert(curr_piece != NO_PIECE);

        for (int i = 1; i <= 2 && ss->ply >= i; i++)
        {
            Move prev_move = (ss - i)->current_move;
            Piece prev_piece = board.pieceAt(prev_move.from());
            assert(prev_piece != NO_PIECE);

            int16_t &score = cont_history[prev_piece][prev_move.to()][curr_piece][move.to()];
            score += bonus - score * std::abs(bonus) / 16384;
        }
    }

} // namespace Astra
