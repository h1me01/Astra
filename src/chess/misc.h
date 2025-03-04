#pragma once

#include <vector>
#include <sstream>
#include <cassert>
#include "types.h"

namespace Chess
{

    void printBitboard(U64 b);

    Square squareFromString(std::string_view square_str);

    std::vector<std::string> split(const std::string &str, char del);

    inline PieceType typeOfPromotion(const MoveType mt)
    {
        switch (mt)
        {
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

    inline bool isProm(const Move &m)
    {
        assert(m != NO_MOVE);
        return m.type() >= PQ_KNIGHT;
    }

    inline bool isCap(const Move &m)
    {
        assert(m != NO_MOVE);
        return m.type() == CAPTURE || m.type() == EN_PASSANT || m.type() >= PC_KNIGHT;
    }

    // prints the move
    std::ostream &operator<<(std::ostream &os, const Move &m);

    // gets the opposite color
    constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

    constexpr Piece makePiece(Color c, PieceType pt) { return Piece(pt + 6 * c); }

    constexpr PieceType typeOf(Piece pc) { return PIECE_TO_PIECE_TYPE[pc]; }

    constexpr Color colorOf(Piece pc)
    {
        assert(pc != NO_PIECE);
        return pc < 6 ? WHITE : BLACK;
    }

    inline Square &operator++(Square &s) { return s = Square(int(s) + 1); }

    constexpr Square operator+(Square sq, Direction d)
    {
        Square _sq = Square(int(sq) + int(d));
        assert(_sq >= a1 && _sq <= h8);
        return _sq;
    }

    constexpr Square operator-(Square sq, Direction d)
    {
        Square _sq = Square(int(sq) - int(d));
        assert(_sq >= a1 && _sq <= h8);
        return _sq;
    }

    constexpr Rank rankOf(Square sq)
    {
        assert(sq >= a1 && sq <= h8);
        return Rank(sq >> 3);
    }

    constexpr File fileOf(Square sq)
    {
        assert(sq >= a1 && sq <= h8);
        return File(sq & 0b111);
    }

    // gets the diagonal (a1 to h8) of the square
    constexpr int diagOf(Square sq)
    {
        assert(sq >= a1 && sq <= h8);
        return 7 + rankOf(sq) - fileOf(sq);
    }

    // gets the anti-diagonal (h1 to a8) of the square
    constexpr int antiDiagOf(Square sq)
    {
        assert(sq >= a1 && sq <= h8);
        return rankOf(sq) + fileOf(sq);
    }

    inline Square &operator+=(Square &s, Direction d) { return s = s + d; }
    inline Square &operator-=(Square &s, Direction d) { return s = s - d; }

    constexpr Square relSquare(Color c, Square sq)
    {
        assert(sq >= a1 && sq <= h8);
        return c == WHITE ? sq : Square(sq ^ 56);
    }

    constexpr Rank relativeRank(Color c, Rank r)
    {
        assert(r >= RANK_1 && r <= RANK_8);
        return c == WHITE ? r : Rank(RANK_8 - r);
    }

} // namespace Chess
