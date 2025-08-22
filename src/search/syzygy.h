#pragma once

#include "../chess/board.h"
#include "../fathom/tbprobe.h"

using namespace Chess;

namespace Search {

unsigned int probe_wdl(const Board &board);

} // namespace Search
