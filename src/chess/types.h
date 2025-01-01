#ifndef TYPES_H
#define TYPES_H

#include <string>
#include <cstdint>

namespace Chess
{
    using U64 = uint64_t;
    using Score = int16_t;

    const std::string PIECE_STR = "PNBRQKpnbrqk.";
    const std::string STARTING_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    constexpr int MAX_PLY = 121;

    const int PIECE_VALUES[] = {100, 325, 325, 500, 1000, 30000, 0};

    // clang-format off
    enum Color
    {
        WHITE,
        BLACK,
        NUM_COLORS = 2
    };

    enum Direction
    {
        NORTH = 8,
        NORTH_EAST = 9,
        EAST = 1,
        SOUTH_EAST = -7,
        SOUTH = -8,
        SOUTH_WEST = -9,
        WEST = -1,
        NORTH_WEST = 7,
        NORTH_NORTH = 16,
        SOUTH_SOUTH = -16
    };

    enum PieceType
    {
        PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
        NO_PIECE_TYPE, NUM_PIECE_TYPES = 6
    };

    constexpr PieceType PIECE_TO_PIECE_TYPE[13] =
    {
        PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
        PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING,
        NO_PIECE_TYPE
    };

    enum Piece
    {
        WHITE_PAWN, WHITE_KNIGHT, WHITE_BISHOP, WHITE_ROOK, WHITE_QUEEN, WHITE_KING,
        BLACK_PAWN, BLACK_KNIGHT, BLACK_BISHOP, BLACK_ROOK, BLACK_QUEEN, BLACK_KING,
        NO_PIECE, NUM_PIECES = 12
    };

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
        NO_SQUARE, NUM_SQUARES = 64
    };

    const std::string SQSTR[65] =
    {
        "a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1",
        "a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
        "a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
        "a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
        "a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
        "a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
        "a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
        "a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
        "None"
    };

    enum File
    {
        FILE_A,
        FILE_B,
        FILE_C,
        FILE_D,
        FILE_E,
        FILE_F,
        FILE_G,
        FILE_H
    };

    enum Rank
    {
        RANK_1,
        RANK_2,
        RANK_3,
        RANK_4,
        RANK_5,
        RANK_6,
        RANK_7,
        RANK_8
    };
    // clang-format on

    constexpr Score VALUE_DRAW = 0;
    constexpr Score VALUE_MATE = 32000;
    constexpr Score VALUE_INFINITE = 32001;
    constexpr Score VALUE_NONE = 32002;
    constexpr Score VALUE_TB_WIN = VALUE_MATE;
    constexpr Score VALUE_TB_WIN_IN_MAX_PLY = VALUE_TB_WIN - MAX_PLY;

    enum MoveType
    {
        QUIET,
        CAPTURE,
        CASTLING,
        EN_PASSANT,
        PQ_KNIGHT, PQ_BISHOP, PQ_ROOK, PQ_QUEEN,
        PC_KNIGHT, PC_BISHOP, PC_ROOK, PC_QUEEN
    };

    // max number of possible legal moves in chess are 218
    // use 128 for faster move generation
    constexpr int MAX_MOVES = 128;

    class Move
    {
    public:
        // default move (a1a1)
        Move() : move(0) {}

        constexpr explicit Move(uint16_t m) : move(m) {}

        constexpr Move(const Move &other) : score(other.score), move(other.move) {}
        constexpr Move(Square from, Square to, MoveType mt) : move(mt << 12 | from << 6 | to) {}

        Square to() const { return Square(move & 0x3f); }
        Square from() const { return Square(move >> 6 & 0x3f); }
        MoveType type() const { return MoveType(move >> 12); }

        Move &operator=(const Move &m)
        {
            if (this != &m)
            {
                move = m.move;
                score = m.score;
            }
            return *this;
        }

        uint16_t raw() const { return move; }

        bool operator==(const Move &m) const { return move == m.move; }
        bool operator!=(const Move &m) const { return move != m.move; }
        bool operator!() const { return move == 0; }

        int score = 0;

    private:
        // first 4 bits represent the move flag
        // next 6 bits represent the to square
        // last 6 bits represent the from square
        uint16_t move;
    };

    const auto NULL_MOVE = Move(65);
    const auto NO_MOVE = Move();

} // namespace Chess

#endif // TYPES_H
