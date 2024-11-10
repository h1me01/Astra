#include "uci.h"
#include <bitset>

int main()
{
    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();

    NNUE::nnue.init();

    UCI::Uci uci;
    uci.loop();

    return 0;
}
