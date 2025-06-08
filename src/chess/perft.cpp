#include "perft.h"
#include "movegen.h"
#include <chrono>
#include <iostream>

namespace Chess {

U64 perft(Board &board, int depth) {
    if(depth == 0)
        return 1;

    MoveList<> ml;
    ml.gen<LEGALS>(board);

    if(depth == 1)
        return ml.size();

    U64 nodes = 0;
    for(const Move &move : ml) {
        board.makeMove(move, false);
        nodes += perft(board, depth - 1);
        board.unmakeMove(move);
    }

    return nodes;
}

void testPerft(Board &board, int depth) {
    if(depth < 1) {
        std::cout << "Invalid depth value.\n";
        return;
    }

    std::cout << "\nPerft test at depth " << depth << ":\n\n";
    auto start = std::chrono::high_resolution_clock::now();

    MoveList<> ml;
    ml.gen<LEGALS>(board);

    U64 total_nodes = 0;
    for(const Move &move : ml) {
        board.makeMove(move, false);
        U64 nodes = perft(board, depth - 1);
        board.unmakeMove(move);

        total_nodes += nodes;
        std::cout << move << ": " << nodes << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = end - start;
    double time_ms = diff.count();

    std::cout << "\nTotal nodes: " << total_nodes << std::endl;
    std::cout << "Total time: " << time_ms << "ms\n\n";
}

} // namespace Chess
