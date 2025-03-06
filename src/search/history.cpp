#include "history.h"
#include "search.h"
#include "tune.h"
#include <algorithm>
#include <cstring>

namespace Astra
{
    // helper

    int getFormula(int value, int bonus)
    {
        return bonus - value * std::abs(bonus) / 16384;
    }

    void updateCorrection(int16_t &value, int diff, int depth)
    {
        const int bonus = std::clamp(diff * depth / 8, -256, 256);
        value = bonus - int(value) * std::abs(bonus) / 1024;
    }

    int historyBonus(int depth)
    {
        return std::min(int(max_history_bonus), history_mult * depth - history_minus);
    }

    // history
    void History::update(const Board &board, Move &best, Stack *ss, Move *q_moves, int qc, Move *c_moves, int cc, int depth)
    {
        Color stm = board.getTurn();
        int bonus = historyBonus(depth);

        if (!isCap(best))
        {
            // don't set quiet queen promotions as a counter/killer,
            // so we don't actually return it twice in the movepicker
            if (best.type() != PQ_QUEEN)
            {
                Move prev_move = (ss - 1)->curr_move;
                if (isValidMove(prev_move))
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
            if (isValidMove((ss - offset)->curr_move))
            {
                Piece pc = (ss - offset)->moved_piece;
                assert(pc >= WHITE_PAWN && pc <= BLACK_KING);

                int16_t &value = (*(ss - offset)->conth)[pc][move.to()];
                value += getFormula(value, bonus);
            }
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

    void History::updateMaterialCorr(const Board &board, Score raw_eval, Score real_score, int depth)
    {
        int diff = real_score - raw_eval;

        updateCorrection(pawn_corr[board.getTurn()][CORR_IDX(board.getPawnHash())], diff, depth);
        updateCorrection(w_non_pawn_corr[board.getTurn()][CORR_IDX(board.getNonPawnHash(WHITE))], diff, depth);
        updateCorrection(b_non_pawn_corr[board.getTurn()][CORR_IDX(board.getNonPawnHash(BLACK))], diff, depth);
    }

    void History::updateContCorr(Score raw_eval, Score real_score, int depth, const Stack *ss)
    {
        const Move prev_move = (ss - 1)->curr_move;
        const Move pprev_move = (ss - 2)->curr_move;

        Piece prev_pc = (ss - 1)->moved_piece;
        Piece pprev_pc = (ss - 2)->moved_piece;

        if (!prev_move || !pprev_move)
            return;

        updateCorrection(cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()], real_score - raw_eval, depth);
    }

} // namespace Astra
