#include "chess/board.h"

using namespace Chess;

int main() {
    init_lookup_tables();
    Zobrist::init();

    Board board(STARTING_FEN);
    board.perft(5);
    
    return 0;
}
