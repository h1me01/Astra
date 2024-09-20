#include "datagen.h"
#include "chess/perft.h"
#include "uci.h"

int main() {
    std::cout << "Tsukuyomi Chess Engine by Semih Oezalp" << std::endl;

    initLookUpTables();
    Zobrist::init();
    Tsukuyomi::initReductions();

    // generate data for neural network
    //saveNetInput(fenToInput(loadDataset()));

    // test performance and correctness of move generation
    //testPerft(5);

    UCI::Uci uci;
    uci.loop();

    return 0;
}
