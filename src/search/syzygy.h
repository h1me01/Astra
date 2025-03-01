#pragma once

#include "../chess/board.h"

namespace Astra
{

    Score probeWDL(const Board &board);

    std::pair<Score, Move> probeDTZ(const Board &board);

} // namespace Astra
