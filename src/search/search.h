#ifndef SEARCH_H
#define SEARCH_H

#include "tt.h"
#include "history.h"
#include "timeman.h"

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

        Search(const std::string &fen);

        Move bestMove();

    private:
        int root_depth = 0;
        
        PVLine pv_table[MAX_PLY + 1];
        History history;
        TimeMan tm;

        Score aspSearch(int depth, Score prev_eval, Stack *ss);
        Score negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node, const Move skipped = NO_MOVE);
        Score qSearch(int depth, Score alpha, Score beta, Stack *ss);

        int adjustEval(const Board &board, const Stack *ss, Score eval) const;
        bool isLimitReached(int depth) const;
        void updatePv(Move move, int ply);
        void printUciInfo(Score result, int depth, PVLine &pv_line) const;
    };

} // namespace Astra

#endif // SEARCH_H
