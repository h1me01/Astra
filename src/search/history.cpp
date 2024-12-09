#include "history.h"
#include "search.h"

#include <cstring> // for memset
#include "tune.h"

namespace Astra
{
    PARAM(history_mult, 150, 130, 180, 8);
    PARAM(history_minus, 37, 10, 50, 10);
    PARAM(max_history_bonus, 1646, 1500, 1800, 50);

    int historyBonus(int depth)
    {
        return std::min(int(max_history_bonus), history_mult * depth - history_minus);
    }

    int History::getHistoryHeuristic(Color stm, Move move) const
    {
        Square from = move.from();
        Square to = move.to();
        return hh[stm][from][to];
    }

    int History::getQuietHistory(const Board& board, const Stack* ss, Move &move) const
    {
        Square from = move.from();
        Square to = move.to();
        Piece pc = board.pieceAt(from);

        assert(pc != NO_PIECE);

        std::cout <<  (ss - 4)->cont_history[pc][to] << std::endl;

        return hh[board.getTurn()][from][to] +
                (ss - 1)->cont_history[pc][to] +
                (ss - 2)->cont_history[pc][to] +
                (ss - 4)->cont_history[pc][to];
    }

    int History::getCaptureHistory(const Board &board, Move &move) const
    {
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(move.to()));
        Piece pc = board.pieceAt(move.from());
        
        assert(pc != NO_PIECE);
        assert(captured != NO_PIECE_TYPE);
        
        return ch[pc][move.to()][captured];
    }

    Move History::getCounterMove(Move prev_move) const
    {
        Square from = prev_move.from();
        Square to = prev_move.to();

        assert(from >= a1 && from <= h8);
        assert(to >= a1 && to <= h8);

        return counters[from][to];
    }

    void History::update(Board &board, Move &best, Stack *ss, Move *q_moves, int qc, Move *c_moves, int cc, int depth)
    {
        Color stm = board.getTurn();
        int bonus = historyBonus(depth);

        if (!board.isCap(best))
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
        int16_t &score = hh[c][move.from()][move.to()];
        score += bonus - score * std::abs(bonus) / 16384;
    }

    void History::updateCH(Board &board, Move &move, int bonus)
    {
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(move.to()));
        Piece pc = board.pieceAt(move.from());

        assert(captured != NO_PIECE_TYPE);
        assert(pc != NO_PIECE);

        int16_t &score = ch[pc][move.to()][captured];
        score += bonus - score * std::abs(bonus) / 16384;
    }

    void History::updateContH(Board &board, Move &move, Stack *ss, int bonus)
    {
        Piece pc = board.pieceAt(move.from());
        if (pc == NO_PIECE)
            pc = board.pieceAt(move.to());

        assert(pc != NO_PIECE);

        for (int offset : {1, 2, 4, 6})
            if ((ss - offset)->curr_move != NO_MOVE)
            {
                int16_t &score = (ss - offset)->cont_history[pc][move.to()];
                score += bonus - score * std::abs(bonus) / 16384;
            }
    }

} // namespace Astra
