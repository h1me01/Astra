#ifndef MISC_H
#define MISC_H

#include <vector>
#include <sstream>
#include <cassert>
#include "types.h"

namespace Chess
{
    // helper to print bitboards for debugging
    void printBitboard(U64 b);

    // helper to get square from string
    Square squareFromString(std::string_view square_str);

    // helper to split the fen
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

    inline bool isProm(const Move &m) { return m.type() >= PQ_KNIGHT; }

    inline bool isCap(const Move &m) 
    { 
        return m.type() == CAPTURE || m.type() == EN_PASSANT || m.type() >= PC_KNIGHT; 
    }

    // prints the move
    std::ostream &operator<<(std::ostream &os, const Move &m);

    // gets the opposite color
    constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

    constexpr Piece makePiece(Color c, PieceType pt) { return Piece(pt + 6 * c); }

    constexpr PieceType typeOf(Piece p) { return PIECE_TO_PIECE_TYPE[p]; }

    constexpr Color colorOf(Piece p) { return p < 6 ? WHITE : BLACK; }

    inline Square &operator++(Square &s) { return s = Square(int(s) + 1); }

    constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
    constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }

    constexpr Rank rankOf(Square s) { return Rank(s >> 3); }
    constexpr File fileOf(Square s) { return File(s & 0b111); }

    // gets the diagonal (a1 to h8) of the square
    constexpr int diagOf(Square s) { return 7 + rankOf(s) - fileOf(s); }
    // gets the anti-diagonal (h1 to a8) of the square
    constexpr int antiDiagOf(Square s) { return rankOf(s) + fileOf(s); }

    inline Square &operator+=(Square &s, Direction d) { return s = s + d; }
    inline Square &operator-=(Square &s, Direction d) { return s = s - d; }

    constexpr Square relativeSquare(Color c, Square s) { return Square(s ^ (c * 56)); }

    constexpr Rank relativeRank(Color c, Rank r) { return c == WHITE ? r : Rank(RANK_8 - r); }

    constexpr U64 ooBlockersMask(Color c) { return c == WHITE ? 0x60 : 0x6000000000000000; }
    constexpr U64 oooBlockersMask(Color c) { return c == WHITE ? 0xe : 0xE00000000000000; }

} // namespace Chess

#endif // MISC_H
