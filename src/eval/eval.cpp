#include "eval.h"
#include "nnue.h"
#include <algorithm> // std::clamp

namespace NNUE {

    Score evaluate(Board& board) {
        Color stm = board.getTurn();
        Accumulator acc = board.getAccumulator();
        const int32_t eval = nnue.forward(acc, stm);
        Score score = std::clamp(eval, -VALUE_MIN_MATE + 1, VALUE_MIN_MATE - 1);
        return score;
    }

} // namespace Eval
