#include "history.h"
#include "search.h"
#include "tune.h"

#include <algorithm>

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
            // only update quiet history if best move was important
            if (depth > 3 || qc > 1)
            {
                updateHistoryScore(hh[stm][best.from()][best.to()], bonus);
                updateContH(best, ss, bonus);

                // quiet maluses
                for (int i = 0; i < qc; i++)
                {
                    Move quiet = q_moves[i];
                    if (quiet == best)
                        continue;
                    updateHistoryScore(hh[stm][quiet.from()][quiet.to()], -bonus);
                    updateContH(quiet, ss, -bonus);
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

    void History::updateQuietHistory(Color c, Move move, int bonus)
    {
        hh[c][move.from()][move.to()] += bonus;
    }

    void History::updateContH(Move &move, Stack *ss, int bonus)
    { 
        for (int offset : {1, 2, 4, 6})
            if ((ss - offset)->curr_move != NO_MOVE && (ss - offset)->curr_move != NULL_MOVE)
                updateHistoryScore((*(ss - offset)->conth)[(ss - offset)->moved_piece][move.to()], bonus);
    }

    void History::updatePawnHistory(Score raw_eval, Score real_score, int depth, const Board& board)
    {
        const int16_t corr = std::clamp((real_score - raw_eval) * depth / 8, -256, 256);
        int16_t& value = pawn_corr[board.getTurn()][board.getPawnHash() % 32768];
        value += corr - int(value) * abs(corr) / 1024;
    }

    void History::updateContCorr(Score raw_eval, Score real_score, int depth, const Stack* ss)
    {
        const Move prev_move = (ss - 1)->curr_move;
        const Move pprev_move = (ss - 2)->curr_move;

        Piece prev_pc = (ss - 1)->moved_piece;
        Piece pprev_pc = (ss - 2)->moved_piece;

        if (!prev_move || !pprev_move)
            return;

        const int16_t corr = std::clamp((real_score - raw_eval) * depth / 8, -256, 256);
        int16_t& value = cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()];
        value += corr - int(value) * abs(corr) / 1024;
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

} // namespace Astra
