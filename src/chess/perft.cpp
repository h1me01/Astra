#include <chrono>

#include "../util.h"
#include "movegen.h"
#include "perft.h"

namespace astra {

uint64_t perft_node(Board& board, int depth) {
    MoveList<Move> ml;
    gen_moves<GenType::LEGAL>(ml, board);

    if (depth == 0)
        return 1;
    if (depth == 1)
        return ml.size();

    uint64_t nodes = 0;
    for (const auto& move : ml) {
        board.make_move(move);
        nodes += perft_node(board, depth - 1);
        board.undo_move(move);
    }
    return nodes;
}

void perft(Board& board, int depth) {
    if (depth < 1) {
        println("Invalid depth value: {}", depth);
        return;
    }

    auto start = std::chrono::high_resolution_clock::now();

    MoveList<Move> ml;
    gen_moves<GenType::LEGAL>(ml, board);

    uint64_t total_nodes = 0;
    for (const auto& move : ml) {
        board.make_move(move);
        uint64_t nodes = perft_node(board, depth - 1);
        board.undo_move(move);
        total_nodes += nodes;
        println("{}: {}", move, nodes);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = end - start;
    double time_ms = diff.count();

    println("\nTotal nodes: {}", total_nodes);
    println("Total time : {} ms\n", time_ms);
}

} // namespace astra
