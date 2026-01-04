#pragma once

#include "../chess/misc.h"
#include "history.h"

using namespace chess;

namespace search {

using PieceToContinuation = int16_t[NUM_PIECES + 1][NUM_SQUARES];

struct PVLine {
    Move pv[MAX_PLY + 1];
    uint8_t length = 0;

    PVLine() = default;

    PVLine(const PVLine &other) : length(other.length) {
        for(int i = 0; i < length; i++)
            pv[i] = other.pv[i];
    }

    PVLine &operator=(const PVLine &other) {
        if(this != &other) {
            length = other.length;
            for(int i = 0; i < length; i++)
                pv[i] = other.pv[i];
        }
        return *this;
    }

    Move &operator[](int depth) {
        return pv[depth];
    }

    Move operator[](int depth) const {
        return pv[depth];
    }
};

struct Stack {
    int ply = 0;
    int made_moves = 0;

    Move move = Move::none();
    Move killer = Move::none();
    Move skipped = Move::none();

    Piece moved_piece = NO_PIECE;
    Score static_eval = SCORE_NONE;

    PVLine pv;
    PieceToContinuation *cont_hist = nullptr;
    PieceToContinuation *cont_corr_hist = nullptr;
};

} // namespace search
