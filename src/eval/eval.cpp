#include "eval.h"
#include "nnue.h"
#include <algorithm> // std::clamp

namespace Eval {

    Score evaluate(Board& board) {
        Color stm = board.getTurn();
        NNUE::Accumulator acc = board.getAccumulator();

        int32_t eval = NNUE::nnue.forward(acc, stm);

        return std::clamp(eval, -VALUE_MIN_MATE + 1, VALUE_MIN_MATE - 1);
    }

} // namespace Eval
