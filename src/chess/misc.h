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
    std::vector<std::string> split(const std::string& str, char del);

    inline PieceType typeOfPromotion(const MoveType mt)
    {
        switch (mt)
        {
        case PR_KNIGHT: return KNIGHT;
        case PR_BISHOP: return BISHOP;
        case PR_ROOK: return ROOK;
        case PR_QUEEN: return QUEEN;
        default: return NO_PIECE_TYPE;
        }
    }

    inline bool isProm(const Move& m) { return m.type() >= PR_KNIGHT; }

    // prints the move
    std::ostream& operator<<(std::ostream& os, const Move& m);

    // gets the opposite color
    constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

    constexpr Piece makePiece(Color c, PieceType pt)
    {
        assert(pt != NO_PIECE_TYPE);
        return Piece(pt + 6 * c);
    }

    constexpr PieceType typeOf(Piece p) { return PIECE_TO_PIECE_TYPE[p]; }

    constexpr Color colorOf(Piece p) {  return p < 6 ? WHITE : BLACK; }

    inline Square& operator++(Square& s) { return s = Square(int(s) + 1); }

    constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
    constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }

    constexpr Rank rankOf(Square s) { return Rank(s >> 3); }
    constexpr File fileOf(Square s) { return File(s & 0b111); }

    // gets the diagonal (a1 to h8) of the square
    constexpr int diagOf(Square s) { return 7 + rankOf(s) - fileOf(s); }
    // gets the anti-diagonal (h1 to a8) of the square
    constexpr int antiDiagOf(Square s) { return rankOf(s) + fileOf(s); }

    inline Square& operator+=(Square& s, Direction d) { return s = s + d; }
    inline Square& operator-=(Square& s, Direction d) { return s = s - d; }

    constexpr Square relativeSquare(Color c, Square s) { return Square(s ^ (c * 56)); }

    constexpr Rank relativeRank(Color c, Rank r) 
    {
        return c == WHITE ? r : Rank(RANK_8 - r);
    }

    constexpr Direction relativeDir(Color c, Direction d)
    {
        return Direction(c == WHITE ? d : -d);
    }

} // namespace Chess

#endif //MISC_H
