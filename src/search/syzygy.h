#ifndef SYZYGY_H
#define SYZYGY_H

#include "../chess/board.h"

namespace Astra
{

    Score probeWDL(const Board &board);

    std::pair<Score, Move> probeDTZ(const Board &board);

} // namespace Astra

#endif // SYZYGY_H
