#ifndef SEARCH_H
#define SEARCH_H

#include "tt.h"
#include "history.h"
#include "timeman.h"

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

    void initReductions();

    class Search
    {
    public:
        int id = 0; // main thread

        bool use_tb = false;

        U64 nodes = 0;
        U64 tb_hits = 0;
        int sel_depth = 0;

        Limits limit;
        Board board;

        Search(const std::string &fen) : board(fen)
        {
            tt.clear();
        }

        Move bestMove();

    private:
        int root_depth = 0;

        PVLine pv_table[MAX_PLY + 1];
        History history;
        TimeMan tm;

        U64 move_nodes[NUM_SQUARES][NUM_SQUARES]{};

        Score aspSearch(int depth, Score prev_eval, Stack *ss);
        Score negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node, const Move skipped = NO_MOVE);
        Score qSearch(int depth, Score alpha, Score beta, Stack *ss);

        int adjustEval(const Board &board, const Stack *ss, Score eval) const;
        bool isLimitReached(int depth) const;
        void updatePv(Move move, int ply);
    };

    inline int Search::adjustEval(const Board &board, const Stack *ss, Score eval) const
    {
        eval += history.getMaterialCorr(board) + history.getContCorr(ss);
        return std::clamp(eval, Score(-VALUE_TB_WIN_IN_MAX_PLY + 1), Score(VALUE_TB_WIN_IN_MAX_PLY - 1));
    }

} // namespace Astra

#endif // SEARCH_H
