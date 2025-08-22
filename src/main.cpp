#include "chess/board.h"
#include "nnue/nnue.h"
#include "uci.h"

int main(int argc, char **argv) {
    Chess::init_lookup_tables();
    Zobrist::init();
    Cuckoo::init();
    NNUE::nnue.init();

    Search::init_reductions();

    UCI::UCI uci;
    uci.loop(argc, argv);

    return 0;
}
