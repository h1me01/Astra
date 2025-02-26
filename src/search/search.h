#ifndef SEARCH_H
#define SEARCH_H

#include "tt.h"
#include "history.h"
#include "timeman.h"
#include "../chess/movegen.h"

#include <algorithm>

namespace Astra
{

    struct PVLine
    {
        Move pv[MAX_PLY + 1];
        uint8_t length = 0;

        Move &operator[](int depth) { return pv[depth]; }
        Move operator[](int depth) const { return pv[depth]; }
    };

    struct RootMove
    {
        Move move;
        Score score;
        Score avg_score;
        int seldepth;
        U64 nodes;
        PVLine pv;
    };

    void initReductions();

    class Search
    {
    public:
        int id = 0; // main thread

        bool use_tb = false;

        U64 nodes = 0;
        U64 tb_hits = 0;
        int seldepth = 0;

        Limits limit;
        Board board;

        Search(const std::string &fen) : board(fen)
        {
            tt.clear();
        }

        Move bestMove();

    private:
        int multipv_idx, root_depth;

        MoveList<RootMove> root_moves;

        PVLine pv_table[MAX_PLY + 1];
        History history;
        TimeMan tm;

        Score aspSearch(int depth, Stack *ss);
        Score negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node, const Move skipped = NO_MOVE);
        Score qSearch(int depth, Score alpha, Score beta, Stack *ss);

        int adjustEval(const Board &board, const Stack *ss, Score eval) const;

        bool isLimitReached(int depth) const;
        void updatePv(Move move, int ply);

        void sortRootMoves(int offset);
        bool foundRootMove(const Move &move);

        void printUciInfo();
    };

    inline int Search::adjustEval(const Board &board, const Stack *ss, Score eval) const
    {
        eval += history.getMaterialCorr(board) + history.getContCorr(ss);
        return std::clamp(eval, Score(-VALUE_TB_WIN_IN_MAX_PLY + 1), Score(VALUE_TB_WIN_IN_MAX_PLY - 1));
    }

    inline void Search::sortRootMoves(int offset)
    {
        for (int i = offset; i < root_moves.size(); i++)
        {
            int best = i;
            for (int j = i + 1; j < root_moves.size(); j++)
                if (root_moves[j].score > root_moves[i].score)
                    best = j;

            if (best != i)
                std::swap(root_moves[i], root_moves[best]);
        }
    }

    inline bool Search::foundRootMove(const Move &move)
    {
        for (int i = multipv_idx; i < root_moves.size(); i++)
            if (root_moves[i].move == move)
                return true;
        return false;
    }

} // namespace Astra

#endif // SEARCH_H
