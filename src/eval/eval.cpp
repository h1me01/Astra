#include "eval.h"
#include "nnue.h"
#include <algorithm> // std::clamp

namespace Eval {

    Score evaluate(const Board& board) {
        Color stm = board.getTurn();
        const int32_t eval = NNUE::nnue.forward(board.getAccumulator(), stm);
        Score score = std::clamp(eval, -VALUE_MIN_MATE + 1, VALUE_MIN_MATE - 1);
        return score;
    }

} // namespace Eval
