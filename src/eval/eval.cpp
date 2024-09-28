#include "eval.h"
#include "nnue.h"
#include <algorithm> // std::clamp

namespace Eval {

    Score evaluate(const Board& board) {
        Color stm = board.getTurn();
        const int v = NNUE::nnue.forward(board.getAccumulator(), stm);
        Score score = std::clamp(v, -VALUE_MIN_MATE + 1, VALUE_MIN_MATE - 1);
        return score;
    }

} // namespace Eval
