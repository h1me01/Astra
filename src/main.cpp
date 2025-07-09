#include "uci.h"

int main(int argc, char **argv) {
    init_lookup_tables();
    Zobrist::init();
    Cuckoo::init();

    Astra::init_reductions();
    NNUE::nnue.init();

    UCI::Uci uci;
    uci.loop(argc, argv);

    return 0;
}
