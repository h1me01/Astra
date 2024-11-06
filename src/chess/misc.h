#ifndef MISC_H
#define MISC_H

#include <vector>
#include <sstream>
#include "types.h"

namespace Chess
{
    // helper to print bitboards for debugging
    void printBitboard(U64 b);

    // helper to get square from string
    Square squareFromString(std::string_view square_str);

    // helper to split the fen
    std::vector<std::string> split(const std::string& str, char del);

    inline PieceType typeOfPromotion(const MoveFlags mf)
    {
        switch (mf)
        {
        case PR_KNIGHT:
        case PC_KNIGHT:
            return KNIGHT;
        case PR_BISHOP:
        case PC_BISHOP:
            return BISHOP;
        case PR_ROOK:
        case PC_ROOK:
            return ROOK;
        case PR_QUEEN:
        case PC_QUEEN:
            return QUEEN;
        default:
            return NO_PIECE_TYPE;
        }
    }

    inline bool isCapture(const Move& m)
    {
        const MoveFlags flag = m.flag();
        return flag == CAPTURE || flag == EN_PASSANT || (flag >= PC_KNIGHT && flag <= PC_QUEEN);
    }

    inline bool isPromotion(const Move& m)
    {
        const MoveFlags flag = m.flag();
        return flag >= PR_KNIGHT && flag <= PC_QUEEN;
    }

    // prints the move
    std::ostream& operator<<(std::ostream& os, const Move& m);

    // gets the opposite color
    constexpr Color operator~(Color c)
    {
        return Color(c ^ BLACK);
    }

    constexpr Piece makePiece(Color c, PieceType pt)
    {
        if (pt == NO_PIECE_TYPE)
            return NO_PIECE;
        return Piece(pt + 6 * c);
    }

    constexpr PieceType typeOf(Piece p)
    {
        return PIECE_TO_PIECE_TYPE[p];
    }

    constexpr Color colorOf(Piece p)
    {
        return p < 6 ? WHITE : BLACK;
    }

    inline Square& operator++(Square& s)
    {
        s = Square(int(s) + 1);
        return s;
    }

    constexpr Square operator+(Square s, Direction d)
    {
        return Square(int(s) + int(d));
    }

    constexpr Square operator-(Square s, Direction d)
    {
        return Square(int(s) - int(d));
    }

    inline Square& operator+=(Square& s, Direction d)
    {
        return s = s + d;
    }

    inline Square& operator-=(Square& s, Direction d)
    {
        return s = s - d;
    }

    constexpr Rank rankOf(Square s)
    {
        return Rank(s >> 3);
    }

    constexpr File fileOf(Square s)
    {
        return File(s & 0b111);
    }

    // gets the diagonal (a1 to h8) of the square
    constexpr int diagOf(Square s)
    {
        return 7 + rankOf(s) - fileOf(s);
    }

    // gets the anti-diagonal (h1 to a8) of the square
    constexpr int antiDiagOf(Square s)
    {
        return rankOf(s) + fileOf(s);
    }

    constexpr Rank relativeRank(Color c, Rank r)
    {
        return c == WHITE ? r : Rank(RANK_8 - r);
    }

    constexpr Direction relativeDir(Color c, Direction d)
    {
        return Direction(c == WHITE ? d : -d);
    }

    constexpr Square mirrorVertically(Color c, Square s)
    {
        return c == WHITE ? s : Square(s ^ 56);
    }

} // namespace Chess

#endif //MISC_H
