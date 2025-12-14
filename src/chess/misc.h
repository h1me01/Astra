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
    return score != VALUE_NONE;
}

// gets opposite color
constexpr Color operator~(Color c) {
    assert(valid_color(c));
    return Color(c ^ BLACK);
}

constexpr Piece make_piece(Color c, PieceType pt) {
    assert(valid_color(c));
    assert(valid_piece_type(pt));
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
constexpr int sq_anti_diag(Square sq) {
    assert(valid_sq(sq));
    return sq_rank(sq) + sq_file(sq);
}

inline Square &operator+=(Square &sq, Direction d) {
    sq = sq + d;
    assert(valid_sq(sq));
    return sq;
}

inline Square &operator-=(Square &sq, Direction d) {
    sq = sq - d;
    assert(valid_sq(sq));
    return sq;
}

constexpr Square rel_sq(Color c, Square sq) {
    assert(valid_sq(sq));
    assert(valid_color(c));
    return c == WHITE ? sq : Square(sq ^ 56);
}

constexpr Rank rel_rank(Color c, Rank r) {
    assert(valid_color(c));
    assert(r >= RANK_1 && r <= RANK_8);
    return c == WHITE ? r : Rank(RANK_8 - r);
}

constexpr Score mate_in(int ply) {
    assert(ply >= 0);
    return VALUE_MATE - ply;
}

constexpr Score mated_in(int ply) {
    assert(ply >= 0);
    return -VALUE_MATE + ply;
}

constexpr bool is_loss(const Score score) {
    assert(valid_score(score));
    return score <= VALUE_TB_LOSS_IN_MAX_PLY;
}

constexpr bool is_win(const Score score) {
    assert(valid_score(score));
    return score >= VALUE_TB_WIN_IN_MAX_PLY;
}

constexpr bool is_decisive(const Score score) {
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

constexpr Square ep_sq(Square sq) {
    assert(valid_sq(sq));
    return Square(sq ^ 8);
}

inline std::ostream &operator<<(std::ostream &os, Square sq) {
    if(sq == NO_SQUARE)
        return os << "NO SQUARE";
    char file_char = 'a' + sq_file(sq);
    char rank_char = '1' + sq_rank(sq);
    return os << file_char << rank_char;
}

inline std::ostream &operator<<(std::ostream &os, Piece pc) {
    return os << PIECE_STR[pc];
}

inline std::ostream &operator<<(std::ostream &os, const Move &move) {
    if(move == NO_MOVE)
        os << "NO MOVE";
    else if(move == NULL_MOVE)
        os << "NULL MOVE";
    else
        os << move.from() << move.to();

    if(move.is_prom()) {
        const PieceType pt = move.prom_type();
        os << Piece(pt + 6);
    }

    return os;
}

} // namespace chess
