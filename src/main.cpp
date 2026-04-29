#include "chess/bitboard.h"
#include "chess/cuckoo.h"
#include "chess/zobrist.h"
#include "datagen/gen_fens.h"
#include "nnue/nnue.h"
#include "uci/uci.h"

using namespace astra;

int main(int argc, char** argv) {
    bitboards::init();
    zobrist::init();
    cuckoo::init();
    nnue::nnue.init();

    if (argc >= 2 && std::string(argv[1]).find("genfens") != std::string::npos) {
        datagen::generate_fens(argc, argv);
    } else {
        uci::UCI uci;
        uci.loop(argc, argv);
    }

    return 0;
}
