#pragma once

#include "../chess/misc.h"

using namespace Chess;

namespace Engine {

using ContH = int16_t[NUM_PIECES + 1][NUM_SQUARES + 1];

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
    int made_moves;

    Move move = NO_MOVE;
    Move killer = NO_MOVE;
    Move skipped = NO_MOVE;

    Piece moved_piece = NO_PIECE;
    Score static_eval = VALUE_NONE;

    PVLine pv;
    ContH *conth;
};

} // namespace Engine
