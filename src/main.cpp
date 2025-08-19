#include "chess/board.h"
#include "eval/eval.h"
#include "search/search.h"

using namespace Chess;

int main() {
    init_lookup_tables();
    Zobrist::init();
    Eval::init_tables();

    Search::Search search;
    search.start();

    return 0;
}
