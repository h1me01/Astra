#include "uci/uci.h"

int main(int argc, char** argv) {
    bitboards::init();
    zobrist::init();
    cuckoo::init();
    nnue::nnue.init();

    uci::UCI uci;
    uci.loop(argc, argv);

    return 0;
}
