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

        for (const Move &move: moves) {
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.unmakeMove(move);
        }

        return nodes;
    }

    void testPerft(int max_depth) {
        for (const auto &testCase: test_cases) {
            Board board(testCase.fen);

            std::cout << "\nFen: " << testCase.fen << std::endl;

            for (size_t i = 0; i < testCase.nodes.size(); ++i) {
                const int depth = static_cast<int>(i) + 1;

                auto start = std::chrono::high_resolution_clock::now();
                auto nodes = perft(board, depth);
                auto end = std::chrono::high_resolution_clock::now();

                std::chrono::duration<double, std::milli> diff = end - start;
                std::cout << "Depth: " << depth << " | Nodes: " << nodes << " | Time: " << diff.count() << " | ";

                if (nodes == testCase.nodes[i]) {
                    std::cout << "Test passed!" << std::endl;
                } else {
                    std::cerr << "Test failed! Expected nodes: " << testCase.nodes[i] << std::endl;
                }

                if (depth >= max_depth) {
                    break;
                }
            }
        }
    }

} // namespace Chess
