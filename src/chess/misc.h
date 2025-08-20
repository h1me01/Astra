#pragma once

#include <cassert>
#include <sstream>
#include <vector>

#include "types.h"

namespace Chess {

void print_bb(U64 b);

Square sq_from(std::string_view square_str);

std::vector<std::string> split(const std::string &str, char del);

PieceType prom_type(const MoveType mt);

std::ostream &operator<<(std::ostream &os, const Move &m);

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
