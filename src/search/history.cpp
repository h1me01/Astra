#include "history.h"
#include "search.h"

#include <cstring> // for memset
#include "tune.h"

namespace Astra
{
    PARAM(history_mult, 140, 100, 200, 8);
    PARAM(history_minus, 25, 10, 40, 5);
    PARAM(max_history_bonus, 1707, 1100, 1800, 50);

    int historyBonus(int depth)
    {
        return std::min(int(max_history_bonus), history_mult * depth - history_minus);
    }

    History::History()
    {
        std::memset(quiet_history, 0, sizeof(quiet_history));
        std::memset(cont_history, 0, sizeof(cont_history));
        std::memset(capt_history, 0, sizeof(capt_history));

        // set to NO_MOVE
        std::memset(counters, 0, sizeof(counters));

        // set to with NO_MOVE
        std::fill_n(killer1, MAX_PLY, NO_MOVE);
        std::fill_n(killer2, MAX_PLY, NO_MOVE);
    }

    int History::getQHScore(Color c, Move &move) const
    {
        return quiet_history[c][move.from()][move.to()];
    }

    int History::getCHScore(const Board &board, Move &move) const
    {
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(move.to()));
        Piece p = board.pieceAt(move.from());
        assert(p != NO_PIECE); 
        assert(captured != NO_PIECE_TYPE);
        return capt_history[p][move.to()][captured];
    }

    int History::getContHScore(const Board &board, Move &move, const Move &prev_move) const
    {
        Piece p = board.pieceAt(move.from());
        Piece prev_p = board.pieceAt(prev_move.from());
        return cont_history[prev_p][prev_move.to()][p][move.to()];
    }

    Move History::getCounterMove(Move move) const
    {
        return counters[move.from()][move.to()];
    }

    void History::update(Board &board, Move &best, Stack *ss, Move *q_moves, int qc, Move *c_moves, int cc, int depth)
    {
        int bonus = historyBonus(depth);

        if (!board.isCapture(best))
        {
            Move prev_move = (ss - 1)->curr_move;
            if (prev_move != NO_MOVE)
                counters[prev_move.from()][prev_move.to()] = best;

            if (best != killer1[ss->ply]) {
                killer2[ss->ply] = killer1[ss->ply];
                killer1[ss->ply] = best; 
            }

            Color c = board.getTurn();
            if (depth > 1)
            {
                updateHH(best, c, bonus);
                updateContH(board, best, ss, bonus);
            }

            // history maluses
            for (int i = 0; i < qc; i++)
            {
                Move quiet = q_moves[i];
                if (quiet == best)
                    continue;

                updateHH(quiet, c, -bonus);
                updateContH(board, quiet, ss, -bonus);
            }
        }
        else
            updateCH(board, best, bonus);

        // update history for captures
        for (int i = 0; i < cc; i++)
        {
            Move cap = c_moves[i];
            if (cap == best)
                continue;

            updateCH(board, cap, -bonus);
        }
    }

    void History::updateHH(Move &move, Color c, int bonus)
    {
        int16_t &score = quiet_history[c][move.from()][move.to()];
        score += bonus - score * std::abs(bonus) / 16384;
    }

    void History::updateCH(Board &board, Move &move, int bonus)
    {
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(move.to()));
        Piece curr_piece = board.pieceAt(move.from());

        int16_t &score = capt_history[curr_piece][move.to()][captured];
        score += bonus - score * std::abs(bonus) / 16384;
    }

    void History::updateContH(Board &board, Move &move, Stack *ss, int bonus)
    {
        Piece curr_piece = board.pieceAt(move.from());

        int offsets[] = {-1, -2, -4, -6}; 
        for (int offset : offsets) {
            if ((ss + offset)->curr_move != NO_MOVE) {
                Move prev_move = (ss + offset)->curr_move;
                Piece prev_piece = board.pieceAt(prev_move.from());

                int16_t &score = cont_history[prev_piece][prev_move.to()][curr_piece][move.to()];
                score += bonus - score * std::abs(bonus) / 16384;
            }
        }
    }

    int16_t History::quiet_history[NUM_COLORS][NUM_SQUARES][NUM_SQUARES]{};

    int16_t History::cont_history[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECES + 1][NUM_SQUARES]{};

    int16_t History::capt_history[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECE_TYPES]{};

} // namespace Astra
