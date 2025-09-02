#include "uci/uci.h"

int main(int argc, char **argv) {
    Chess::init_lookup_tables();
    Zobrist::init();
    Cuckoo::init();
    NNUE::nnue.init();
    Engine::init_reductions();

    UCI::UCI uci;
    uci.loop(argc, argv);

    return 0;
}
