#include "uci.h"

int main() {
    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();
    NNUE::nnue.init("nn-768-2x256-1.nnue");

    UCI::Uci uci;
    uci.loop();

    return 0;
}
