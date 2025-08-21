#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

namespace Chess {

using U64 = uint64_t;
using Score = int16_t;

const std::string PIECE_STR = "PNBRQKpnbrqk.";
const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

const int PIECE_VALUES[] = {100, 325, 325, 500, 900, 0, 0};

constexpr int MAX_PLY = 128;

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
    NORTH_EAST = 9,
    EAST = 1,
    SOUTH_EAST = -7,
    SOUTH = -8,
    SOUTH_WEST = -9,
    WEST = -1,
    NORTH_WEST = 7
};

constexpr int NUM_PIECE_TYPES = 6;
enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE_TYPE };

// clang-format off
constexpr PieceType PIECE_TO_PIECE_TYPE[] = {
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,         
    PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,         
    NO_PIECE_TYPE 
};
// clang-format on

// clang-format off
constexpr int NUM_PIECES = 12;
enum Piece {
    WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
    BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING,
    NO_PIECE
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

const std::string SQSTR[] =
{
    "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
    "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
    "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
    "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
    "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
    "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
    "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
    "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
    "NO SQUARE"
};
// clang-format on

enum File { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H };

enum Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8 };

// clang-format off
enum MoveType
{
    QUIET,
    CAPTURE,
    CASTLING,
    EN_PASSANT,
    PQ_KNIGHT, PQ_BISHOP, PQ_ROOK, PQ_QUEEN,
    PC_KNIGHT, PC_BISHOP, PC_ROOK, PC_QUEEN
};
// clang-format on

// max number of possible legal moves in chess are 218
// use 128 for faster move generation
constexpr int MAX_MOVES = 128;

class Move {

  public:
    // default move (a1a1)
    Move() : data(0), score(0) {}

    explicit Move(uint16_t m) : data(m), score(0) {}

    Move(const Move &other) : data(other.data), score(other.score) {}

    Move(Square from, Square to, MoveType mt) //
        : data((mt << 12) | (to << 6) | from), score(0) {}

    Square from() const {
        return Square(data & 0x3f);
    }

    Square to() const {
        return Square((data >> 6) & 0x3f);
    }

    MoveType type() const {
        return MoveType(data >> 12);
    }

    Move &operator=(const Move &m) {
        if(this != &m)
            data = m.data;
        return *this;
    }

    uint16_t raw() const {
        return data;
    }

    void set_score(int score) {
        this->score = score;
    }

    int get_score() const {
        return score;
    }

    bool operator==(const Move &m) const {
        return data == m.data;
    }

    bool operator!=(const Move &m) const {
        return data != m.data;
    }

    bool is_valid() const {
        return data != 0 && data != 65;
    }

    bool is_prom() const {
        assert(is_valid());
        return type() >= PQ_KNIGHT;
    }

    bool is_underprom() const {
        assert(is_valid());
        return type() == PQ_KNIGHT || type() == PQ_BISHOP || type() == PC_KNIGHT || type() == PC_BISHOP;
    }

    bool is_quiet() const {
        return !is_cap() && !is_prom();
    }

    bool is_cap() const {
        assert(is_valid());
        return type() == CAPTURE || type() == EN_PASSANT || type() >= PC_KNIGHT;
    }

  private:
    // first 6 bits represent the from square
    // next 6 bits represent the to square
    // last 4 bits represent the move type
    uint16_t data;
    int score;
};

const Move NO_MOVE{};
const Move NULL_MOVE{65};

} // namespace Chess
