#include <iostream>
#include "uci.h"

int main(int argc, char **argv)
{
    std::cout << "Astra chess engine written by Semih Özalp" << std::endl;

    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();

    NNUE::nnue.init();

    UCI::Uci uci;
    uci.loop(argc, argv);

    return 0;
}
