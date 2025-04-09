#include "uci.h"

int main(int argc, char **argv)
{
    initLookUpTables();
    Zobrist::init();
    Cuckoo::init();

    Astra::initReductions();
    NNUE::nnue.init();

    UCI::Uci uci;
    uci.loop(argc, argv);

    return 0;
}
