#include "eval.h"
#include "nnue.h"
#include <algorithm> // std::clamp

namespace Eval
{

    Score evaluate(Board &board)
    {
        int32_t eval = NNUE::nnue.forward(board.getAccumulator(), board.getTurn());

        // scale based on phase
        eval = (128 + board.getPhase()) * eval / 128;

        return std::clamp(eval, -(VALUE_MATE - MAX_PLY) + 1, VALUE_MATE - MAX_PLY - 1);
    }

} // namespace Eval
