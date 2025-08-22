#include <chrono>

#include "board.h"
#include "movegen.h"

namespace Chess {

void Board::perft(int depth) {
    if(depth < 1) {
        std::cout << "Invalid depth value.\n";
        return;
    }

    auto perft = [&](Board &b, int d, auto &&perft_ref) -> U64 {
        if(d == 0)
            return 1;

        MoveList<> ml;
        ml.gen<LEGALS>(b);

        if(d == 1)
            return ml.size();

        U64 nodes = 0;
        for(const Move &move : ml) {
            b.make_move(move);
            nodes += perft_ref(b, d - 1, perft_ref);
            b.unmake_move(move);
        }
        return nodes;
    };

    std::cout << "\nPerft test at depth " << depth << ":\n\n";
    auto start = std::chrono::high_resolution_clock::now();

    MoveList<> ml;
    ml.gen<LEGALS>(*this);

    U64 total_nodes = 0;
    for(const Move &move : ml) {
        make_move(move);
        U64 nodes = perft(*this, depth - 1, perft);
        unmake_move(move);

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
