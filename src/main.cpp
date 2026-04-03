#include "uci/uci.h"

void test_fen(std::string fen) {
    Board board(fen);
    nnue::AccumList acc_list;
    acc_list.reset(board);
    std::cout << "fen: " << fen << "\n";
    std::cout << "eval: " << nnue::nnue.forward(board, acc_list.back()) << "\n";
}

int main(int argc, char** argv) {
    bitboards::init();
    zobrist::init();
    cuckoo::init();
    nnue::nnue.init();

    uci::UCI uci;
    uci.loop(argc, argv);

    return 0;
}
