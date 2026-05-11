#pragma once

#include <cassert>
#include <cstdlib>

#include "../chess/types.h"
#include "../ndarray.h"

namespace astra::search {

using Score = int;
using PieceToContinuation = NDArray<int16_t, NUM_PIECES + 1, NUM_SQUARES>;

constexpr int MAX_PLY = 128;

constexpr Score SCORE_DRAW = 0;
constexpr Score SCORE_NONE = 32002;
constexpr Score SCORE_INFINITE = 32001;

constexpr Score SCORE_MATE = 32000;
constexpr Score SCORE_MATE_IN_MAX_PLY = SCORE_MATE - MAX_PLY;

constexpr Score SCORE_TB = SCORE_MATE_IN_MAX_PLY - 1;
constexpr Score SCORE_TB_WIN_IN_MAX_PLY = SCORE_TB - MAX_PLY;
constexpr Score SCORE_TB_LOSS_IN_MAX_PLY = -SCORE_TB_WIN_IN_MAX_PLY;

constexpr bool valid_score(Score score) { return score > -SCORE_INFINITE && score < SCORE_INFINITE; }

constexpr Score mate_in(int ply) {
    assert(ply >= 0);
    return SCORE_MATE - ply;
}

constexpr Score mated_in(int ply) { return -mate_in(ply); }

constexpr bool is_win(Score score) {
    assert(valid_score(score) || std::abs(score) == SCORE_INFINITE);
    return score >= SCORE_TB_WIN_IN_MAX_PLY;
}

constexpr bool is_loss(Score score) {
    assert(valid_score(score) || std::abs(score) == SCORE_INFINITE);
    return score <= SCORE_TB_LOSS_IN_MAX_PLY;
}

constexpr bool is_decisive(Score score) { return is_loss(score) || is_win(score); }

struct PVLine {
    NDArray<Move, MAX_PLY> pv;
    uint8_t length = 0;

    Move& operator()(int depth) {
        assert(depth >= 0 && depth < MAX_PLY);
        return pv(depth);
    }

    Move operator()(int depth) const {
        assert(depth >= 0 && depth < MAX_PLY);
        return pv(depth);
    }
};

struct Stack {
    int ply = 0;
    int move_count = 0;

    Move move = Move::none();
    Move skipped = Move::none();

    Piece moved_piece = NO_PIECE;
    Score static_eval = SCORE_NONE;

    PVLine pv;

    PieceToContinuation* cont_hist = nullptr;
    PieceToContinuation* cont_corr_hist = nullptr;
};

} // namespace astra::search
