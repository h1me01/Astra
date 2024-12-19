#include "history.h"
#include "search.h"
#include "tune.h"

namespace Astra
{
    void updateHistoryScore(int16_t &score, int bonus)
    {
        score += bonus - score * std::abs(bonus) / 16384;
    }

    int historyBonus(int depth)
    {
        return std::min(int(max_history_bonus), history_mult * depth - history_minus);
    }

    int History::getHistoryHeuristic(Color stm, Move move) const
    {
        return hh[stm][move.from()][move.to()];
    }

    int History::getQuietHistory(const Board &board, const Stack *ss, Move &move) const
    {
        Square from = move.from();
        Square to = move.to();
        Piece pc = board.pieceAt(from);

        assert(pc != NO_PIECE);

        return hh[board.getTurn()][from][to] + (ss - 1)->conth[pc][to] + (ss - 2)->conth[pc][to] + (ss - 4)->conth[pc][to];
    }

    int History::getCapHistory(const Board &board, Move &move) const
    {
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(move.to()));
        Piece pc = board.pieceAt(move.from());

        assert(pc != NO_PIECE);
        assert(captured != NO_PIECE_TYPE);

        return ch[pc][move.to()][captured];
    }

    Move History::getCounterMove(Move prev_move) const
    {
        return counters[prev_move.from()][prev_move.to()];
    }

    void History::update(const Board &board, Move &best, Stack *ss, Move *q_moves, int qc, Move *c_moves, int cc, int depth)
    {
        Color stm = board.getTurn();
        int bonus = historyBonus(depth);

        if (!isCap(best))
        {
            // don't set quiet promotion queen as a count/killer,
            // so we don't actually return it twice in the movepicker
            if (best.type() != PQ_QUEEN)
            {
                Move prev_move = (ss - 1)->curr_move;
                if (prev_move != NO_MOVE)
                    counters[prev_move.from()][prev_move.to()] = best;

                ss->killer = best;
            }

            // credits to ethereal
            // only update best move history if it was important
            if (depth > 3 || qc > 1)
            {
                updateHistoryScore(hh[stm][best.from()][best.to()], bonus);
                updateContH(board, best, ss, bonus);

                // quiet maluses
                for (int i = 0; i < qc; i++)
                {
                    Move quiet = q_moves[i];
                    if (quiet == best)
                        continue;
                    updateHistoryScore(hh[stm][quiet.from()][quiet.to()], -bonus);
                    updateContH(board, quiet, ss, -bonus);
                }
            }
        }
        else
            updateCapHistory(board, best, bonus);

        // capture maluses
        for (int i = 0; i < cc; i++)
        {
            Move cap = c_moves[i];
            if (cap == best)
                continue;
            updateCapHistory(board, cap, -bonus);
        }
    }

    void History::updateCapHistory(const Board &board, Move &move, int bonus)
    {
        Square to = move.to();
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(to));
        Piece pc = board.pieceAt(move.from());

        assert(captured != NO_PIECE_TYPE);
        assert(pc != NO_PIECE);

        updateHistoryScore(ch[pc][to][captured], bonus);
    }

    void History::updateContH(const Board &board, Move &move, Stack *ss, int bonus)
    {
        Square to = move.to();
        Piece pc = board.pieceAt(move.from());
        // when updating cont history in lmr we have to take the to square
        // since we have already made the move
        if (pc == NO_PIECE)
            pc = board.pieceAt(to);

        assert(pc != NO_PIECE);

        for (int offset : {1, 2, 4, 6})
            if ((ss - offset)->curr_move != NO_MOVE)
                updateHistoryScore((ss - offset)->conth[pc][to], bonus);
    }

} // namespace Astra
