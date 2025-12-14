#include "uci/uci.h"

int main(int argc, char **argv) {
    bitboards::init();
    zobrist::init();
    cuckoo::init();
    nnue::nnue.init();
    search::init_reductions();

    uci::UCI uci;
    uci.loop(argc, argv);

    return 0;
}
