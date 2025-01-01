#include "uci.h"

int main(int argc, char **argv)
{
    initLookUpTables();
    Zobrist::init();
    Astra::initReductions();

    NNUE::nnue.init();
    /*
    Board board(STARTING_FEN);

    auto start = std::chrono::system_clock::now();

    while (true)
    {
        NNUE::nnue.putPiece(board.getAccumulator(), WHITE_PAWN, a2, e1, e7, WHITE);
        count++;

        if (count == 100000000)
            break;
    }

    // End time in ms
    auto end = std::chrono::system_clock::now();
    double ms = std::chrono::duration<double>(end - start).count() * 1000; // Convert seconds to ms

    std::cout << "Time: " << ms << "ms\n";
    */
    UCI::Uci uci;
    uci.loop(argc, argv);

    return 0;
}
