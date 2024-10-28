#ifndef SEARCH_H
#define SEARCH_H

#include <iostream>
#include <thread>
#include "tt.h"
#include "pvtable.h"
#include "timemanager.h"
#include "moveordering.h"
#include "../chess/board.h"

namespace Astra
{
    enum Node
    {
        NON_PV, PV, ROOT 
    };

    struct Stack
    {
        int move_count;
        Move current_move;
        Move excluded_move;
        Score static_eval;
        uint16_t ply;
    };

    struct SearchResult
    {
        Move best_move = NO_MOVE;
        Score score = -VALUE_INFINITE;
    };

    void initReductions();

    class Search
    {
    public:
        int id = 0; // main thread

        U64 nodes = 0;
        U64 tb_hits = 0;
        uint8_t sel_depth = 0;

        Limits limit;

        Board board;
        MoveOrdering move_ordering;
        TimeManager time_manager;

        bool use_tb = false;

        explicit Search(const std::string& fen);

        SearchResult start();

        void reset();
        void stop();

    private:
        PVTable pv_table;

        bool isLimitReached(int depth) const;
        void printUciInfo(Score result, int depth, PVLine& pv_line) const;

        Score qSearch(Score alpha, Score beta, Node node, Stack* ss);
        Score abSearch(int depth, Score alpha, Score beta, Node node, Stack* ss);
        Score aspSearch(int depth, Score prev_eval, Stack* ss);
    };

    class ThreadPool
    {
    public:
        ThreadPool() : stop(false) {}

        void start(const Board& board, const Limits& limit, int worker_count, bool use_tb);
        void kill();

        U64 getTotalNodes() const;
        U64 getTotalTbHits() const;
        uint8_t getSelDepth() const;

        // stop running all threads
        bool stop;

    private:
        std::vector<Search> pool;
        std::vector<std::thread> running_threads;
    };

    // global variables
    extern ThreadPool threads;
    extern TTable tt;

} // namespace Astra

#endif //SEARCH_H
