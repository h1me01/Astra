#ifndef EVAL_H
#define EVAL_H

#include "../chess/board.h"

using namespace Chess;

namespace Eval {

   Score evaluate(const Board& board);

} // namespace Eval

#endif //EVAL_H