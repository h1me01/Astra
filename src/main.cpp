#include "chess/board.h"
#include "eval/eval.h"
#include "uci.h"

int main() {
    Chess::init_lookup_tables();
    Zobrist::init();
    Eval::init_tables();
    Search::init_reductions();

    Board board(STARTING_FEN);

    UCI::UCI uci;
    uci.loop();

    return 0;
}
