#pragma once

#include <cassert>
#include <sstream>
#include <vector>

#include "types.h"

namespace chess {

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
    return score > -SCORE_INFINITE && score < SCORE_INFINITE;
}

constexpr bool valid_file(File f) {
    return f >= FILE_A && f <= FILE_H;
}

constexpr bool valid_rank(Rank r) {
    return r >= RANK_1 && r <= RANK_8;
}

// gets opposite color
constexpr Color operator~(Color c) {
    assert(valid_color(c));
    return Color(c ^ BLACK);
}

constexpr Square make_square(Rank r, File f) {
    assert(valid_rank(r) && valid_file(f));
    return Square((r << 3) + f);
}

constexpr Piece make_piece(Color c, PieceType pt) {
    assert(valid_color(c) && valid_piece_type(pt));
    return Piece(pt + 6 * c);
}

constexpr PieceType piece_type(Piece pc) {
    assert(valid_piece(pc) || pc == NO_PIECE);
    return PIECE_TO_PIECE_TYPE[pc];
}

constexpr Color piece_color(Piece pc) {
    assert(valid_piece(pc));
    return Color(pc > 5);
}

inline Square &operator++(Square &sq) {
    return sq = Square(int(sq) + 1);
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

constexpr Square &operator+=(Square &sq, Direction d) {
    sq = sq + d;
    assert(valid_sq(sq));
    return sq;
}

constexpr Square &operator-=(Square &sq, Direction d) {
    sq = sq - d;
    assert(valid_sq(sq));
    return sq;
}

constexpr Square ep_sq(Square sq) {
    assert(valid_sq(sq));
    return Square(sq ^ 8);
}

constexpr Rank sq_rank(Square sq) {
    assert(valid_sq(sq));
    return Rank(sq >> 3);
}

constexpr File sq_file(Square sq) {
    assert(valid_sq(sq));
    return File(sq & 0b111);
}

constexpr Square rel_sq(Color c, Square sq) {
    assert(valid_color(c) && valid_sq(sq));
    return c == WHITE ? sq : Square(sq ^ 56);
}

constexpr Rank rel_rank(Color c, Rank r) {
    assert(valid_color(c) && valid_rank(r));
    return c == WHITE ? r : Rank(RANK_8 - r);
}

constexpr Score mate_in(int ply) {
    assert(ply >= 0);
    return SCORE_MATE - ply;
}

constexpr Score mated_in(int ply) {
    return -mate_in(ply);
}

constexpr bool is_win(Score score) {
    assert(valid_score(score) || std::abs(score) == SCORE_INFINITE);
    return score >= SCORE_TB_WIN_IN_MAX_PLY;
}

constexpr bool is_loss(Score score) {
    assert(valid_score(score) || std::abs(score) == SCORE_INFINITE);
    return score <= SCORE_TB_LOSS_IN_MAX_PLY;
}

constexpr bool is_decisive(Score score) {
    return is_loss(score) || is_win(score);
}

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
        } else {
            current += c;
        }
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

inline std::ostream &operator<<(std::ostream &os, Square sq) {
    if(sq == NO_SQUARE)
        return os << "no square";
    return os << char('a' + sq_file(sq)) << char('1' + sq_rank(sq));
}

inline std::ostream &operator<<(std::ostream &os, Piece pc) {
    return os << PIECE_STR[pc];
}

inline std::ostream &operator<<(std::ostream &os, Move move) {
    if(move.is_none())
        return os << "none";
    else if(move.is_null())
        return os << "null";
    else
        os << move.from() << move.to();

    if(move.is_prom()) {
        const PieceType pt = move.prom_type();
        os << Piece(pt + 6);
    }

    return os;
}

} // namespace chess
