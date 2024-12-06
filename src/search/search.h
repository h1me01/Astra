#ifndef SEARCH_H
#define SEARCH_H

#include "tt.h"
#include "history.h"
#include "timeman.h"

namespace Astra
{
    struct Stack
    {
        uint16_t ply;
        Score static_eval = VALUE_NONE;
        
        Move skipped = NO_MOVE;
        Move killer1 = NO_MOVE;
        Move killer2 = NO_MOVE;
        Move curr_move = NO_MOVE;

        int16_t cont_history[NUM_PIECES + 1][NUM_SQUARES] {};
    };

    struct PVLine
    {
        Move pv[MAX_PLY + 1];
        uint8_t length = 0;

        Move& operator[](int depth) { return pv[depth]; }
        Move operator[](int depth) const { return pv[depth]; }
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
        int root_depth = 0;
        PVLine pv_table[MAX_PLY + 1];
       
        TimeMan tm; 

        Score aspSearch(int depth, Score prev_eval, Stack *ss);
        Score negamax(int depth, Score alpha, Score beta, Stack *ss, bool cut_node);
        Score qSearch(int depth, Score alpha, Score beta, Stack *ss);

        void updatePV(int ply, Move& m);

        bool isLimitReached(int depth) const;
        void printUciInfo(Score result, int depth, PVLine &pv_line) const;
    };

    // global variable
    extern TTable tt;

} // namespace Astra

#endif // SEARCH_H
