#include "eval.h"
#include "nnue.h"
#include <algorithm> // std::clamp

namespace Eval {

    Score evaluate(const Board& board) {
        Color stm = board.getTurn();
        const int v = NNUE::nnue.forward(board.getAccumulator()[stm]);
        Score score = std::clamp(v, VALUE_MATED_IN_PLY + 1, VALUE_MATE_IN_PLY - 1);
        return score;
    }

} // namespace Eval
