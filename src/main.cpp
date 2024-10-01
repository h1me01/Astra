#include "datagen.h"
#include "chess/perft.h"
#include "uci.h"

int main() {
    const std::string version = "2.2";
    std::cout << "Astra " << version << " Chess Engine by Semih Oezalp" << std::endl;

    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();

    // generate data for neural network
    //saveNetInput(fenToInput(loadDataset()));

    // test performance and correctness of move generation
    //testPerft(5);

    UCI::Uci uci;
    uci.loop();

    return 0;
}
