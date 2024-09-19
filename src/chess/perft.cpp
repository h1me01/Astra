#include "perft.h"
#include <chrono>
#include "movegen.h"

namespace Chess {

    U64 perft(Board &board, int depth) {
        U64 nodes = 0;
        MoveList moves(board);

        if (depth == 1) {
            return moves.size();
        }

        for (const Move& move : moves) {
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.unmakeMove(move);
        }

        return nodes;
    }

    void testPerft(int max_depth) {
        if (max_depth < 1 || max_depth > std::size(test_cases)) {
            std::cerr << "Invalid depth for Perft!" << std::endl;
            return;
        }

        for (const auto& testCase : test_cases) {
            Board board(testCase.fen);

            std::cout << "\nFen: " << testCase.fen << std::endl;

            for (int depth = 1; depth <= max_depth; ++depth) {
                auto start = std::chrono::high_resolution_clock::now();
                auto nodes = perft(board, depth);
                auto end = std::chrono::high_resolution_clock::now();

                if (nodes != testCase.results[depth - 1].second) {
                    std::cerr << "Test failed! Expected Nodes: " << testCase.results[depth - 1].second << std::endl;
                    std::cerr << "Actual Nodes: " << nodes << std::endl;
                    std::exit(1);
                }

                std::chrono::duration<double, std::milli> diff = end - start;
                std::cout << "Test passed | Depth: " << depth << " | Time: " << diff.count() << " ms\n";
            }
        }

        std::exit(0);
    }

} // namespace Chess
