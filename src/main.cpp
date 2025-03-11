#include "uci.h"

int main(int argc, char **argv)
{
    std::cout << "Astra 5.0 by Semih Oezalp" << std::endl;

    initLookUpTables();
    Zobrist::init();
    Cuckoo::init();

    Astra::initReductions();
    NNUE::nnue.init();

    UCI::Uci uci;
    uci.loop(argc, argv);

    return 0;
}
