#include "uci/uci.h"

int main(int argc, char **argv) {
    Bitboards::init();
    Zobrist::init();
    Cuckoo::init();
    NNUE::nnue.init();
    Engine::init_reductions();

    UCI::UCI uci;
    uci.loop(argc, argv);

    return 0;
}
