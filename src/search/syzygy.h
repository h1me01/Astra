#pragma once

#include "../chess/board.h"

using namespace Chess;

namespace Astra {

Score probe_wdl(const Board &board);

std::pair<Score, Move> probe_dtz(const Board &board);

} // namespace Astra
