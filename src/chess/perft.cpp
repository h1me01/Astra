#include <iostream>
#include <chrono>
#include "perft.h"
#include "movegen.h"

namespace Chess
{

    U64 perft(Board &board, int depth)
    {
        if (depth == 0)
            return 1;
            
        MoveList ml;
        ml.gen<LEGALS>(board);

        if (depth == 1)
            return ml.size();

        U64 nodes = 0;
        for (const Move &move : ml)
        {
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.unmakeMove(move);
        }

        return nodes;
    }

    void testPerft(const Board &board, int depth)
    {
        if (depth < 1)
        {
            std::cout << "Invalid depth value.\n";
            return;
        }

        std::cout << "\nPerft test at depth " << depth << ":\n\n";

        U64 total_nodes = 0;

        auto start = std::chrono::high_resolution_clock::now();

        MoveList ml;
        ml.gen<LEGALS>(board);

        for (const Move &move : ml)
        {
            Board b = board;
            b.makeMove(move);
            U64 nodes = perft(b, depth - 1);
            total_nodes += nodes;

            std::cout << move << ": " << nodes << std::endl;
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> diff = end - start;
        double time_ms = diff.count();

        std::cout << "\nTotal nodes: " << total_nodes << std::endl;
        std::cout << "Total time: " << time_ms << "ms" << std::endl;
        std::cout << "Nodes per second: " << total_nodes * 1000.0 / std::max(1.0, time_ms) << "\n\n";
    }

} // namespace Chess
