#include "history.h"
#include "search.h"

#include <cstring> // for memset

namespace Astra
{
    History::History()
    {
        clear();
    }

    void History::clear()
    {
        std::memset(history, 0, sizeof(history));
        std::memset(cont_history, 0, sizeof(cont_history));
        std::memset(capt_history, 0, sizeof(capt_history));

        // set to NO_MOVE
        std::memset(counters, 0, sizeof(counters));

        // set to with NO_MOVE 
        std::fill_n(killer1, MAX_PLY, NO_MOVE);
        std::fill_n(killer2, MAX_PLY, NO_MOVE);
    }

    void History::update(Board &board, Move &move, Move *quiet_moves, Stack *ss, int quiet_count, int depth)
    {
        int hh_bonus = std::min(2047, 153 * depth);
        int ch_bonus = std::min(1669, 11 * depth * depth);

        Move prev_move = (ss - 1)->current_move;
        if (prev_move != NO_MOVE)
            counters[prev_move.from()][prev_move.to()] = move;

        if (!board.isCapture(move))
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

        for (int i = 1; i <= 2 && ss->ply >= i; i++)
        {
            Move prev_move = (ss - i)->current_move;
            Piece prev_piece = board.pieceAt(prev_move.from());

            int16_t &score = cont_history[prev_piece][prev_move.to()][curr_piece][move.to()];
            score += bonus - score * std::abs(bonus) / 16384;
        }
    }

    int16_t History::history[NUM_COLORS][NUM_SQUARES][NUM_SQUARES]{};

    int16_t History::cont_history[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECES + 1][NUM_SQUARES]{};

    int16_t History::capt_history[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECE_TYPES]{};

} // namespace Astra
