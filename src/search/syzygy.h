#pragma once

#include "../chess/board.h"

using namespace Chess;

namespace Astra {

Score probeWDL(const Board &board);

std::pair<Score, Move> probeDTZ(const Board &board);

} // namespace Astra
