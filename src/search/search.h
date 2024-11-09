#ifndef SEARCH_H
#define SEARCH_H

#include <thread>
#include "tt.h"
#include "pvtable.h"
#include "history.h"
#include "timemanager.h"

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
        Score static_eval;
        Move current_move;
        Move excluded_move;
    };

    void initReductions();

    class Search
    {
    public:
        int id = 0; // main thread

        bool use_tb = false;

        Board board;

        U64 nodes = 0;
        U64 tb_hits = 0;
        
        uint8_t sel_depth = 0;

        Limits limit;

        Search(const std::string &fen);

        Move start();

        void reset();
        void stop();

    private:

        PVTable pv_table;
        History history;
       
        TimeManager time_manager;

        Score aspSearch(int depth, Score prev_eval, Stack *ss);
        Score negamax(int depth, Score alpha, Score beta, Node node, Stack *ss);
        Score qSearch(Score alpha, Score beta, Node node, Stack *ss);

        bool isLimitReached(int depth) const;
        void printUciInfo(Score result, int depth, PVLine &pv_line) const;
    };

    class ThreadPool
    {
    public:
        ThreadPool() : stop(false) {}

        void launchWorkers(const Board &board, const Limits &limit, int worker_count, bool use_tb);
        void stopAll();

        U64 getTotalNodes() const;
        U64 getTotalTbHits() const;
        uint8_t getSelDepth() const;

        // stop running all threads
        bool stop;

    private:
        std::vector<Search> threads;
        std::vector<std::thread> running_threads;
    };

    // global variables
    extern ThreadPool threads;
    extern TTable tt;

} // namespace Astra

#endif // SEARCH_H
