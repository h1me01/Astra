#ifndef SEARCH_H
#define SEARCH_H

#include "tt.h"
#include "pvtable.h"
#include "history.h"
#include "timeman.h"

namespace Astra
{
    enum Node
    {
        NON_PV,
        PV,
        ROOT
    };

    struct Stack
    {
        uint16_t ply;
        Score eval = VALUE_NONE;
        Move current_move = NO_MOVE;
        Move excluded_move = NO_MOVE;
    };

    void initReductions();

    class Search
    {
    public:
        int id = 0; // main thread

        bool use_tb = false;

        Board board;
        History history;

        U64 nodes = 0;
        U64 tb_hits = 0;
        
        uint8_t sel_depth = 0;

        Limits limit;

        Search(const std::string &fen);

        Move start();

    private:
        PVTable pv_table;
       
        TimeMan tm; 

        Score aspSearch(int depth, Score prev_eval, Stack *ss);
        Score negamax(int depth, Score alpha, Score beta, bool cut_node, Node node, Stack *ss);
        Score qSearch(Score alpha, Score beta, Node node, Stack *ss);

        bool isLimitReached(int depth) const;
        void printUciInfo(Score result, int depth, PVLine &pv_line) const;
    };

    // global variable
    extern TTable tt;

} // namespace Astra

#endif // SEARCH_H
