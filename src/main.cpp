#include "uci.h"

// BENCH DOES WORK IF YOU DO IT AFTER BUILD;
// BUT DOESNT WORK AFTER YOU TYPE GO -> STOP -> UCINEWGAME
// FIX THIS

int main(int argc, char **argv)
{
    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();

    NNUE::nnue.init();

    UCI::Uci uci;
    uci.loop(argc, argv);

    return 0;
}
