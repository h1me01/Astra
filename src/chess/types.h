#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "../search/tune_params.h"

namespace chess {

using U64 = uint64_t;
using Score = int16_t;

const std::string PIECE_STR = "PNBRQKpnbrqk.";
const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

constexpr int PIECE_VALUES[] = {100, 360, 385, 635, 1200, 0, 0};

constexpr int MAX_PLY = 128;
constexpr int MAX_MOVES = 128;

constexpr Score VALUE_DRAW = 0;
constexpr Score VALUE_NONE = 32002;
constexpr Score VALUE_INFINITE = 32001;

constexpr Score VALUE_MATE = 32000;
constexpr Score VALUE_MATE_IN_MAX_PLY = VALUE_MATE - MAX_PLY;

constexpr Score VALUE_TB = VALUE_MATE_IN_MAX_PLY - 1;
constexpr Score VALUE_TB_WIN_IN_MAX_PLY = VALUE_TB - MAX_PLY;
constexpr Score VALUE_TB_LOSS_IN_MAX_PLY = -VALUE_TB_WIN_IN_MAX_PLY;

constexpr int NUM_COLORS = 2;
enum Color {
    WHITE,
    BLACK,
    BOTH_COLORS = 2,
};

enum Direction {
    NORTH = 8,
    SOUTH = -8,
    EAST = 1,
    WEST = -1,
    NORTH_WEST = 7,
    SOUTH_EAST = -7,
    NORTH_EAST = 9,
    SOUTH_WEST = -9
};

constexpr int NUM_PIECE_TYPES = 6;
enum PieceType {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NO_PIECE_TYPE,
};

// clang-format off
constexpr int NUM_PIECES = 12;
enum Piece {
    WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
    BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING,
    NO_PIECE
};

constexpr PieceType PIECE_TO_PIECE_TYPE[] = {
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,         
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,         
    NO_PIECE_TYPE 
};
// clang-format on

// clang-format off
constexpr int NUM_SQUARES = 64;
enum Square
{
    a1, b1, c1, d1, e1, f1, g1, h1,
    a2, b2, c2, d2, e2, f2, g2, h2,
    a3, b3, c3, d3, e3, f3, g3, h3,
    a4, b4, c4, d4, e4, f4, g4, h4,
    a5, b5, c5, d5, e5, f5, g5, h5,
    a6, b6, c6, d6, e6, f6, g6, h6,
    a7, b7, c7, d7, e7, f7, g7, h7,
    a8, b8, c8, d8, e8, f8, g8, h8,
    NO_SQUARE
};
// clang-format on

enum File {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
};

enum Rank {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
};

// clang-format off
enum MoveType
{
    QUIET,
    CAPTURE,
    CASTLING,
    EN_PASSANT,
    PQ_KNIGHT, PQ_BISHOP, PQ_ROOK, PQ_QUEEN,
    PC_KNIGHT, PC_BISHOP, PC_ROOK, PC_QUEEN,
};
// clang-format on

class Move {
  public:
    // default no move (a1a1)
    Move() : data(0) {}

    explicit constexpr Move(uint16_t m) : data(m) {}

    Move(const Move &other) : data(other.data) {}

    Move(Square from, Square to, MoveType mt) //
        : data((mt << 12) | (to << 6) | from) {}

    Square from() const {
        return Square(data & 0x3f);
    }

    Square to() const {
        return Square((data >> 6) & 0x3f);
    }

    MoveType type() const {
        return MoveType(data >> 12);
    }

    Move &operator=(const Move &move) {
        if(this != &move)
            data = move.data;
        return *this;
    }

    static constexpr Move null() {
        return Move(65);
    }

    static constexpr Move none() {
        return Move(0);
    }

    bool is_null() const {
        return *this == Move::null();
    }

    bool is_none() const {
        return *this == Move::none();
    }

    bool is_valid() const {
        return !is_null() && !is_none();
    }

    bool is_cap() const {
        assert(is_valid());
        return type() == CAPTURE || is_ep() || type() >= PC_KNIGHT;
    }

    bool is_castling() const {
        return type() == CASTLING;
    }

    bool is_ep() const {
        return type() == EN_PASSANT;
    }

    bool is_prom() const {
        assert(is_valid());
        return type() >= PQ_KNIGHT;
    }

    bool is_noisy() const {
        return is_cap() || type() == PQ_QUEEN;
    }

    bool is_quiet() const {
        return !is_noisy();
    }

    PieceType prom_type() const {
        switch(type()) {
        case PQ_KNIGHT:
        case PC_KNIGHT:
            return KNIGHT;
        case PQ_BISHOP:
        case PC_BISHOP:
            return BISHOP;
        case PQ_ROOK:
        case PC_ROOK:
            return ROOK;
        case PQ_QUEEN:
        case PC_QUEEN:
            return QUEEN;
        default:
            return NO_PIECE_TYPE;
        }
    }

    uint16_t raw() const {
        return data;
    }

    bool operator==(const Move &move) const {
        return data == move.data;
    }

    bool operator!=(const Move &move) const {
        return data != move.data;
    }

    bool operator!() const {
        return !is_valid();
    }

    explicit operator bool() const {
        return is_valid();
    }

  private:
    // first 6 bits represent the from square
    // next 6 bits represent the to square
    // last 4 bits represent the move type
    uint16_t data;
};

struct ScoredMove : public Move {
    ScoredMove() : Move(), score(0) {}

    ScoredMove(const Move &move) //
        : Move(move), score(0) {}

    ScoredMove(const ScoredMove &other) //
        : Move(other), score(other.score) {}

    ScoredMove &operator=(const ScoredMove &other) {
        if(this != &other) {
            Move::operator=(other);
            score = other.score;
        }
        return *this;
    }

    ScoredMove(ScoredMove &&other) noexcept //
        : Move(std::move(other)), score(other.score) {}

    ScoredMove &operator=(ScoredMove &&other) noexcept {
        if(this != &other) {
            Move::operator=(std::move(other));
            score = other.score;
        }
        return *this;
    }

    int score;
};

} // namespace chess
