#ifndef SEARCH_H
#define SEARCH_H

#include "tt.h"
#include "history.h"
#include "timeman.h"

namespace Astra
{
    struct Stack
    {
        int ply;
        Score eval = VALUE_NONE;

        Move killer1 = NO_MOVE;
        Move killer2 = NO_MOVE;
        Move curr_move = NO_MOVE;

        int16_t conth[NUM_PIECES + 1][NUM_SQUARES]{};
    };

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
        History history;

        Search(const std::string &fen);

        Move bestMove();

    private:
        int root_depth = 0;
        PVLine pv_table[MAX_PLY + 1];

        TimeMan tm;

        Score aspSearch(int depth, Score prev_eval, Stack *ss);
        Score negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node, const Move skipped = NO_MOVE);
        Score qSearch(int depth, Score alpha, Score beta, Stack *ss);

        int adjustEval(Score eval) const;
        bool isLimitReached(int depth) const;
        void printUciInfo(Score result, int depth, PVLine &pv_line) const;
    };

    // global variable
    extern TTable tt;

} // namespace Astra

#endif // SEARCH_H
