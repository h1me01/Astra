#include "uci.h"

int main() {
    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();

    UCI::Uci uci;
    uci.loop();

    return 0;
}
