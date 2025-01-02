#ifndef HISTORY_H
#define HISTORY_H

#include "stack.h"
#include "../chess/board.h"

namespace Astra
{

    int historyBonus(int depth);

    class History
    {
    public:
        int16_t conth[2][NUM_PIECES][NUM_SQUARES][NUM_PIECES][NUM_SQUARES]{};

        void update(const Board &board, Move &move, Stack *ss, Move *q_moves, int qc, Move *c_moves, int cc, int depth);

        void updateQuietHistory(Color c, Move move, int bonus);
        void updateContH(Move &move, Stack *ss, int bonus);

        void updatePawnHistory(const Board &board, Score raw_eval, Score real_score, int depth);
        void updateWNonPawnHistory(const Board &board, Score raw_eval, Score real_score, int depth);
        void updateBNonPawnHistory(const Board &board, Score raw_eval, Score real_score, int depth);
        void updateContCorr(Score raw_eval, Score real_score, int depth, const Stack *ss);

        Move getCounterMove(Move move) const;

        int getHistoryHeuristic(Color stm, Move move) const;
        int getQuietHistory(const Board &board, const Stack *ss, Move &move) const;
        int getCapHistory(const Board &board, Move &move) const;
        int getPawnCorr(const Board &board) const;
        int getWNonPawnCorr(const Board &board) const;
        int getBNonPawnCorr(const Board &board) const;
        int getContCorr(const Stack *ss) const;

    private:
        Move counters[NUM_SQUARES][NUM_SQUARES];

        int16_t hh[NUM_COLORS][NUM_SQUARES][NUM_SQUARES]{};
        int16_t ch[NUM_PIECES][NUM_SQUARES][NUM_PIECE_TYPES]{};

        int16_t cont_corr[NUM_PIECES][NUM_SQUARES][NUM_PIECES][NUM_SQUARES]{};
        int16_t pawn_corr[NUM_COLORS][16384]{};
        int16_t w_non_pawn_corr[NUM_COLORS][16384]{};
        int16_t b_non_pawn_corr[NUM_COLORS][16384]{};

        void updateCapHistory(const Board &board, Move &move, int bonus);
    };

    inline int History::getHistoryHeuristic(Color stm, Move move) const
    {
        return hh[stm][move.from()][move.to()];
    }

    inline int History::getQuietHistory(const Board &board, const Stack *ss, Move &move) const
    {
        Square from = move.from();
        Square to = move.to();
        Piece pc = board.pieceAt(from);

        assert(pc != NO_PIECE);

        return hh[board.getTurn()][from][to] +
               (int)(*(ss - 1)->conth)[pc][to] +
               (int)(*(ss - 2)->conth)[pc][to] +
               (int)(*(ss - 4)->conth)[pc][to];
    }

    inline int History::getCapHistory(const Board &board, Move &move) const
    {
        PieceType captured = move.type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(move.to()));
        Piece pc = board.pieceAt(move.from());

        assert(pc != NO_PIECE);
        assert(captured != NO_PIECE_TYPE);

        return ch[pc][move.to()][captured];
    }

    inline Move History::getCounterMove(Move prev_move) const
    {
        return counters[prev_move.from()][prev_move.to()];
    }

    inline int History::getPawnCorr(const Board &board) const
    {
        return pawn_corr[board.getTurn()][board.getPawnHash() % 16384] / 512;
    }

    inline int History::getWNonPawnCorr(const Board &board) const
    {
        return w_non_pawn_corr[board.getTurn()][board.getNonPawnHash(WHITE) % 16384] / 512;
    }

    inline int History::getBNonPawnCorr(const Board &board) const
    {
        return b_non_pawn_corr[board.getTurn()][board.getNonPawnHash(BLACK) % 16384] / 512;
    }

    inline int History::getContCorr(const Stack *ss) const
    {
        const Move prev_move = (ss - 1)->curr_move;
        const Move pprev_move = (ss - 2)->curr_move;

        Piece prev_pc = (ss - 1)->moved_piece;
        Piece pprev_pc = (ss - 2)->moved_piece;

        if (!prev_move || !pprev_move)
            return 0;

        return cont_corr[prev_pc][prev_move.to()][pprev_pc][pprev_move.to()] / 512;
    }

} // namespace Astra

#endif // HISTORY_H