#include "chess/board.h"
#include "eval/eval.h"

using namespace Chess;

int main() {
    init_lookup_tables();
    Zobrist::init();
    Eval::init_tables();

    Board board(STARTING_FEN);
    std::cout << Eval::evaluate(board) << std::endl;

    return 0;
}
