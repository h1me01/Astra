#ifndef SEARCH_H
#define SEARCH_H

#include <thread>
#include <atomic>
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
        Move best_move = NO_MOVE;
        Score score = -VALUE_INFINITE;
    };

    void initReductions();

    class Search {
    public:
        int num_workers = 1; // default number of threads
        int id = 0; // main thread

        U64 nodes = 0;
        U64 tb_hits = 0;
        uint8_t sel_depth = 0;

        Limits limit;

        Board board;
        MoveOrdering move_ordering;
        TimeManager time_manager;

        bool use_tb = false;

        explicit Search(std::string fen);

        SearchResult start();

        void reset();
        void stop();

    private:
        PVTable pv_table;

        bool isLimitReached(int depth) const;
        void printUciInfo(Score result, int depth, PVLine& pv_line) const;

        Score qSearch(Score alpha, Score beta, Node node, Stack *ss);
        Score abSearch(int depth, Score alpha, Score beta, Node node, Stack *ss);
        Score aspSearch(int depth, Score prev_eval, Stack *ss);
    };

    struct Thread {
        Search search;

        Thread() : search(STARTING_FEN) {}

        void start() {
            SearchResult result = search.start();
            if (search.id == 0) {
                std::cout << "bestmove " << result.best_move << std::endl;
            }
        }
    };

    class ThreadPool {
    public:
        void start(const Board &board, const Limits &limit, int worker_count, bool use_tb);
        void kill();

        U64 getTotalNodes() const;
        U64 getTotalTbHits() const;
        uint8_t getSelDepth() const;

        // stop running all threads
        bool stop = false;
    private:
        std::vector<Thread> pool;
        std::vector<std::thread> running_threads;
    };

    inline ThreadPool threads;
    inline TTable tt(16);

} // namespace Astra

#endif //SEARCH_H
