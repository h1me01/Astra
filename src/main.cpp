#include "datagen.h"
#include "uci.h"

int main() {
    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();

    // generate data for neural network
    //saveNetInput(fenToInput(loadDataset()));

    UCI::Uci uci;
    uci.loop();

    return 0;
}
