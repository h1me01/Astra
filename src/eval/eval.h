#pragma once

#include "../chess/board.h"

using namespace Chess;

namespace Eval {

void init_tables();
Score evaluate(const Board &board);

} // namespace Eval
