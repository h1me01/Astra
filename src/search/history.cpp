#include "history.h"
#include "search.h"

#include <cstring> // for memset
#include "tune.h"

namespace Astra
{
    PARAM(history_mult, 152, 130, 180, 8);
    PARAM(history_minus, 29, 10, 50, 10);
    PARAM(max_history_bonus, 1645, 1500, 1800, 50);

    int historyBonus(int depth)
    {
        return std::min(int(max_history_bonus), history_mult * depth - history_minus);
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

    Move History::getCounterMove(Move prev_move) const
    {
        return counters[prev_move.from()][prev_move.to()];
    }

    void History::update(Board &board, Move &best, Stack *ss, Move *q_moves, int qc, Move *c_moves, int cc, int depth)
    {
        Color stm = board.getTurn();
        int bonus = historyBonus(depth);

        if (!board.isCapture(best))
        {
            Move prev_move = (ss - 1)->curr_move;
            if (prev_move != NO_MOVE)
                counters[prev_move.from()][prev_move.to()] = best;

            if (best != ss->killer1)
            {
                ss->killer2 = ss->killer1;
                ss->killer1 = best;
            }

            if (depth > 4 || qc > 1)
            {
                updateQH(best, stm, bonus);
                updateContH(board, best, ss, bonus);
            }

            // update quiets
            for (int i = 0; i < qc; i++)
            {
                Move quiet = q_moves[i];
                if (quiet == best)
                    continue;

                updateQH(quiet, stm, -bonus);
                updateContH(board, quiet, ss, -bonus);
            }
        }
        else
            updateCH(board, best, bonus);

        // update captures
        for (int i = 0; i < cc; i++)
        {
            Move cap = c_moves[i];
            if (cap == best)
                continue;
            updateCH(board, cap, -bonus);
        }
    }

    void History::updateQH(Move &move, Color c, int bonus)
    {
        int16_t &score = quiet_history[c][move.from()][move.to()];
        score += bonus - score * std::abs(bonus) / 16384;
    }

    void History::updateCH(Board &board, Move &move, int bonus)
    {
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(move.to()));
        Piece p = board.pieceAt(move.from());

        assert(captured != NO_PIECE_TYPE);
        assert(p != NO_PIECE);

        int16_t &score = capt_history[p][move.to()][captured];
        score += bonus - score * std::abs(bonus) / 16384;
    }

    void History::updateContH(Board &board, Move &move, Stack *ss, int bonus)
    {
        Piece p = board.pieceAt(move.from());
        if (p == NO_PIECE)
            p = board.pieceAt(move.to());

        assert(p != NO_PIECE);

        for (int offset : {1, 2, 4, 6})
            if ((ss - offset)->curr_move != NO_MOVE)
            {
                int16_t &score = (ss - offset)->cont_history[p][move.to()];
                score += bonus - score * std::abs(bonus) / 16384;
            }
    }

} // namespace Astra
