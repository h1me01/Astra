#ifndef SEARCH_H
#define SEARCH_H

#include "tt.h"
#include "pvtable.h"
#include "timemanager.h"
#include "moveordering.h"
#include "../chess/board.h"

using namespace Chess;

namespace Astra {

    enum Node {
        NON_PV, PV, ROOT
    };

    struct Stack {
        int move_count;
        Move current_move;
        Move excluded_move;
        Score static_eval;
        uint16_t ply;
    };

    struct SearchResult {
        Move best_move = NULL_MOVE;
        Score score = -VALUE_INFINITE;
    };

    void initReductions();

    class Search {
    public:
        Board board;
        TTable* tt;
        MoveOrdering move_ordering;
        TimeManager time_manager;

        bool use_TB = false;

        // time_per_move in ms
        explicit Search(const std::string& fen);
        ~Search();

        SearchResult bestMove(int max_depth, unsigned int time_per_move);

        std::string getPv();

        void reset();

    private:
        U64 nodes{};
        uint8_t sel_depth = 0;

        PVTable pv_table;

        Score qSearch(Score alpha, Score beta, Node node, Stack *ss);
        Score abSearch(int depth, Score alpha, Score beta, Node node, Stack *ss);
        Score aspSearch(int depth, Score prev_eval, Stack *ss);
    };

} // namespace Astra

#endif //SEARCH_H
