#include "history.h"
#include "search.h"
#include "tune.h"

#include <algorithm>
#include <cstring>

namespace Astra
{

    int getFormula(int value, int bonus)
    {
        return bonus - value * std::abs(bonus) / 16384;
    }

    int historyBonus(int depth)
    {
        return std::min(int(max_history_bonus), history_mult * depth - history_minus);
    }

    History::History()
    {
        for (int i = 0; i < NUM_SQUARES; i++)
            for (int j = 0; j < NUM_SQUARES; j++)
                counters[i][j] = NO_MOVE;

        memset(conth, 0, sizeof(conth));
        memset(hh, 0, sizeof(hh));
        memset(ch, 0, sizeof(ch));
        memset(cont_corr, 0, sizeof(cont_corr));
        memset(pawn_corr, 0, sizeof(pawn_corr));
        memset(w_non_pawn_corr, 0, sizeof(w_non_pawn_corr));
        memset(b_non_pawn_corr, 0, sizeof(b_non_pawn_corr));
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
                updateQuietHistory(stm, best, bonus);
                updateContH(best, ss, bonus);

                // quiet maluses
                for (int i = 0; i < qc; i++)
                {
                    Move quiet = q_moves[i];
                    if (quiet == best)
                        continue;
                    updateQuietHistory(stm, quiet, -bonus);
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
        int16_t &value = hh[c][move.from()][move.to()];
        value += getFormula(value, bonus);
    }

    void History::updateContH(Move &move, Stack *ss, int bonus)
    {
        for (int offset : {1, 2, 4, 6})
            if ((ss - offset)->curr_move != NO_MOVE && (ss - offset)->curr_move != NULL_MOVE)
            {
                int16_t &value = (*(ss - offset)->conth)[(ss - offset)->moved_piece][move.to()];
                value += getFormula(value, bonus);
            }
    }

    int getCorrection(const int diff, int depth, int value)
    {
        const int bonus = std::clamp(diff * depth / 8, -256, 256);
        return bonus - int(value) * std::abs(bonus) / 1024;
    }

    void History::updatePawnHistory(const Board &board, Score raw_eval, Score real_score, int depth)
    {
        int16_t &value = pawn_corr[board.getTurn()][board.getPawnHash() % 16384];
        value = getCorrection(real_score - raw_eval, depth, value);
    }

    void History::updateWNonPawnHistory(const Board &board, Score raw_eval, Score real_score, int depth)
    {
        int16_t &value = w_non_pawn_corr[board.getTurn()][board.getNonPawnHash(WHITE) % 16384];
        value = getCorrection(real_score - raw_eval, depth, value);
    }

    void History::updateBNonPawnHistory(const Board &board, Score raw_eval, Score real_score, int depth)
    {
        int16_t &value = b_non_pawn_corr[board.getTurn()][board.getNonPawnHash(BLACK) % 16384];
        value = getCorrection(real_score - raw_eval, depth, value);
    }

    void History::updateContCorr(Score raw_eval, Score real_score, int depth, const Stack *ss)
    {
        const Move prev_move = (ss - 1)->curr_move;
        const Move pprev_move = (ss - 2)->curr_move;

        Piece prev_pc = (ss - 1)->moved_piece;
        Piece pprev_pc = (ss - 2)->moved_piece;

        if (!prev_move || !pprev_move)
            return;

        int16_t &value = cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()];
        value += getCorrection((real_score - raw_eval) * 256, depth, value);
    }

    void History::updateCapHistory(const Board &board, Move &move, int bonus)
    {
        Square to = move.to();
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(to));
        Piece pc = board.pieceAt(move.from());

        assert(captured != NO_PIECE_TYPE);
        assert(pc != NO_PIECE);

        int16_t &value = ch[pc][to][captured];
        value += getFormula(value, bonus);
    }

} // namespace Astra
