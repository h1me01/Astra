#include "chess/board.h"
#include "nnue/nnue.h"
#include "uci.h"

int main() {
    Chess::init_lookup_tables();
    Zobrist::init();
    NNUE::nnue.init();

    Search::init_reductions();

    UCI::UCI uci;
    uci.loop();

    return 0;
}
