#pragma once

#include <cassert>
#include <cstdint>
#include <string_view>

namespace astra {

using Bitboard = uint64_t;
using Hash = uint64_t;

constexpr std::string_view PIECE_STR = "PNBRQKpnbrqk.";

constexpr int NUM_COLORS = 2;
enum Color : uint8_t {
    WHITE,
    BLACK,
};

enum Direction : int8_t {
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
enum PieceType : uint8_t {
    PAWN,
    KNIGHT,
    BISHOP,
    ROOK,
    QUEEN,
    KING,
    NO_PIECE_TYPE,
};

constexpr int NUM_PIECES = 12;
enum Piece : uint8_t {
    WHITE_PAWN,
    WHITE_KNIGHT,
    WHITE_BISHOP,
    WHITE_ROOK,
    WHITE_QUEEN,
    WHITE_KING,
    BLACK_PAWN,
    BLACK_KNIGHT,
    BLACK_BISHOP,
    BLACK_ROOK,
    BLACK_QUEEN,
    BLACK_KING,
    NO_PIECE,
};

constexpr int NUM_SQUARES = 64;
// clang-format off
enum Square: uint8_t
{
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8,
    NO_SQUARE
};
// clang-format on

enum File : uint8_t {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
};

enum Rank : uint8_t {
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
};

enum MoveType : uint8_t {
    QUIET,
    CAPTURE,
    CASTLING,
    EN_PASSANT,
    PQ_KNIGHT,
    PQ_BISHOP,
    PQ_ROOK,
    PQ_QUEEN,
    PC_KNIGHT,
    PC_BISHOP,
    PC_ROOK,
    PC_QUEEN,
};

class Move {
  public:
    Move()
        : data_(0) {}

    explicit constexpr Move(uint16_t m)
        : data_(m) {}

    Move(Square from, Square to, MoveType mt)
        : data_((mt << 12) | (to << 6) | from) {}

    Square from() const { return static_cast<Square>(data_ & 0x3f); }
    Square to() const { return static_cast<Square>((data_ >> 6) & 0x3f); }

    MoveType type() const { return static_cast<MoveType>(data_ >> 12); }

    static constexpr Move null() { return Move(65); }
    static constexpr Move none() { return Move(0); }

    bool is_null() const { return *this == Move::null(); }
    bool is_none() const { return *this == Move::none(); }
    bool is_valid() const { return !is_null() && !is_none(); }

    bool is_cap() const {
        assert(is_valid());
        return type() == CAPTURE || is_ep() || type() >= PC_KNIGHT;
    }

    bool is_castling() const { return type() == CASTLING; }
    bool is_ep() const { return type() == EN_PASSANT; }

    bool is_prom() const {
        assert(is_valid());
        return type() >= PQ_KNIGHT;
    }

    bool is_noisy() const { return is_cap() || type() == PQ_QUEEN; }
    bool is_quiet() const { return !is_noisy(); }

    PieceType prom_type() const {
        switch (type()) {
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

    uint16_t raw() const { return data_; }

    bool operator==(const Move move) const { return data_ == move.data_; }
    bool operator!=(const Move move) const { return data_ != move.data_; }
    bool operator!() const { return !is_valid(); }

    explicit operator bool() const { return is_valid(); }

  private:
    // first 6 bits represent the from square
    // next 6 bits represent the to square
    // last 4 bits represent the move type
    uint16_t data_;
};

struct ScoredMove : public Move {
    int score = 0;

    ScoredMove() = default;
    ScoredMove(Move move, int score = 0)
        : Move(move),
          score(score) {}
};

constexpr bool is_valid(Color c) { return c == WHITE || c == BLACK; }
constexpr bool is_valid(Square sq) { return sq >= SQ_A1 && sq <= SQ_H8; }
constexpr bool is_valid(PieceType pt) { return pt >= PAWN && pt <= KING; }
constexpr bool is_valid(Piece pc) { return pc >= WHITE_PAWN && pc <= BLACK_KING; }
constexpr bool is_valid(File f) { return f >= FILE_A && f <= FILE_H; }
constexpr bool is_valid(Rank r) { return r >= RANK_1 && r <= RANK_8; }

constexpr Color operator~(Color c) {
    assert(is_valid(c));
    return static_cast<Color>(c ^ 1);
}

constexpr Square make_square(Rank r, File f) {
    assert(is_valid(r));
    assert(is_valid(f));
    return static_cast<Square>((r << 3) + f);
}

constexpr Piece make_piece(Color c, PieceType pt) {
    assert(is_valid(c));
    assert(is_valid(pt));
    return static_cast<Piece>(pt + 6 * c);
}

constexpr PieceType type_of(Piece pc) {
    assert(is_valid(pc) || pc == NO_PIECE);
    return pc == NO_PIECE ? NO_PIECE_TYPE : static_cast<PieceType>(pc % 6);
}

constexpr Color color_of(Piece pc) {
    assert(is_valid(pc));
    return static_cast<Color>(pc / 6);
}

constexpr Square& operator++(Square& sq) { return sq = static_cast<Square>(static_cast<int>(sq) + 1); }

constexpr Square operator+(Square sq, Direction d) {
    Square _sq = static_cast<Square>(static_cast<int>(sq) + static_cast<int>(d));
    assert(is_valid(_sq));
    return _sq;
}

constexpr Square operator-(Square sq, Direction d) {
    Square _sq = static_cast<Square>(static_cast<int>(sq) - static_cast<int>(d));
    assert(is_valid(_sq));
    return _sq;
}

constexpr Square& operator+=(Square& sq, Direction d) {
    sq = sq + d;
    return sq;
}

constexpr Square& operator-=(Square& sq, Direction d) {
    sq = sq - d;
    return sq;
}

constexpr Square ep_sq(Square sq) {
    assert(is_valid(sq));
    return static_cast<Square>(sq ^ 8);
}

constexpr Rank rank_of(Square sq) {
    assert(is_valid(sq));
    return static_cast<Rank>(sq >> 3);
}

constexpr File file_of(Square sq) {
    assert(is_valid(sq));
    return static_cast<File>(sq & 0b111);
}

constexpr Square relative_sq(Color c, Square sq) {
    assert(is_valid(sq));
    assert(is_valid(c));
    return c == WHITE ? sq : static_cast<Square>(sq ^ 56);
}

constexpr Rank relative_rank(Color c, Rank r) {
    assert(is_valid(r));
    assert(is_valid(c));
    return c == WHITE ? r : static_cast<Rank>(RANK_8 - r);
}

} // namespace astra
