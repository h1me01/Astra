#pragma once

#include <cassert>
#include <sstream>
#include <vector>

#include "types.h"

namespace Chess {

inline void print_bb(const U64 b) {
    for(int rank = 7; rank >= 0; --rank) {
        for(int file = 0; file < 8; ++file) {
            U64 mask = 1ULL << (rank * 8 + file);
            std::cout << ((b & mask) ? "1 " : "0 ");
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

inline std::vector<std::string> split(const std::string &str, char del) {
    std::vector<std::string> parts;
    std::string current;

    for(const char c : str) {
        if(c == del) {
            if(!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else
            current += c;
    }

    if(!current.empty())
        parts.push_back(current);

    return parts;
}

inline Square sq_from(std::string_view square_str) {
    int file = square_str[0] - 97;
    int rank = square_str[1] - 49;
    int index = rank * 8 + file;
    return Square(index);
}

inline PieceType prom_type(const MoveType mt) {
    switch(mt) {
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

inline std::ostream &operator<<(std::ostream &os, Square sq) {
    // clang-format off
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
    return os << SQSTR[sq];
}

inline std::ostream &operator<<(std::ostream &os, Piece pc) {
    return os << PIECE_STR[pc];
}

inline std::ostream &operator<<(std::ostream &os, const Move &m) {
    if(m == NO_MOVE)
        os << "NO MOVE";
    else if(m == NULL_MOVE)
        os << "NULL MOVE";
    else
        os << m.from() << m.to();

    if(m.is_prom()) {
        const PieceType pt = prom_type(m.type());
        os << Piece(pt + 6);
    }

    return os;
}

constexpr bool valid_color(Color c) {
    return c == WHITE || c == BLACK;
}

constexpr bool valid_sq(Square sq) {
    return sq >= a1 && sq <= h8;
}

constexpr bool valid_piece_type(PieceType pt) {
    return pt >= PAWN && pt <= KING;
}

constexpr bool valid_piece(Piece pc) {
    return pc >= WHITE_PAWN && pc <= BLACK_KING;
}

constexpr bool valid_score(Score score) {
    return score != VALUE_NONE;
}

// gets opposite color
constexpr Color operator~(Color c) {
    return Color(c ^ BLACK);
}

constexpr Piece make_piece(Color c, PieceType pt) {
    return Piece(pt + 6 * c);
}

constexpr PieceType piece_type(Piece pc) {
    return PIECE_TO_PIECE_TYPE[pc];
}

constexpr Color piece_color(Piece pc) {
    assert(pc != NO_PIECE);
    return pc < 6 ? WHITE : BLACK;
}

inline Square &operator++(Square &s) {
    return s = Square(int(s) + 1);
}

constexpr Square operator+(Square sq, Direction d) {
    Square _sq = Square(int(sq) + int(d));
    assert(valid_sq(_sq));
    return _sq;
}

constexpr Square operator-(Square sq, Direction d) {
    Square _sq = Square(int(sq) - int(d));
    assert(valid_sq(_sq));
    return _sq;
}

constexpr Rank sq_rank(Square sq) {
    assert(valid_sq(sq));
    return Rank(sq >> 3);
}

constexpr File sq_file(Square sq) {
    assert(valid_sq(sq));
    return File(sq & 0b111);
}

// gets the diagonal (a1 to h8) of the square
constexpr int sq_diag(Square sq) {
    assert(valid_sq(sq));
    return 7 + sq_rank(sq) - sq_file(sq);
}

// gets the anti-diagonal (h1 to a8) of the square
constexpr int sq_antidiag(Square sq) {
    assert(valid_sq(sq));
    return sq_rank(sq) + sq_file(sq);
}

inline Square &operator+=(Square &s, Direction d) {
    return s = s + d;
}

inline Square &operator-=(Square &s, Direction d) {
    return s = s - d;
}

constexpr Square rel_sq(Color c, Square sq) {
    assert(valid_sq(sq));
    return c == WHITE ? sq : Square(sq ^ 56);
}

constexpr Rank rel_rank(Color c, Rank r) {
    assert(r >= RANK_1 && r <= RANK_8);
    return c == WHITE ? r : Rank(RANK_8 - r);
}

inline bool is_loss(const Score score) {
    assert(valid_score(score));
    return score <= VALUE_TB_LOSS_IN_MAX_PLY;
}

inline bool is_win(const Score score) {
    assert(valid_score(score));
    return score >= VALUE_TB_WIN_IN_MAX_PLY;
}

inline bool is_decisive(const Score score) {
    return is_loss(score) || is_win(score);
}

} // namespace Chess
