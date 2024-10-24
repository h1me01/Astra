#ifndef MISC_H
#define MISC_H

#include <iostream>
#include <vector>
#include <sstream>
#include "bitboard.h"
#include "types.h"

namespace Chess {
    // helper to print bitboards for debugging
    inline void printBitboard(const U64 b) {
        for (int rank = 7; rank >= 0; --rank) {
            for (int file = 0; file < 8; ++file) {
                int s = rank * 8 + file;
                U64 mask = 1ULL << s;
                std::cout << ((b & mask) ? "1 " : "0 ");
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    // helper to find square from string
    inline Square squareFromString(std::string_view square_str) {
        int file = square_str[0] - 97;
        int rank = square_str[1] - 49;
        int index = rank * 8 + file;
        return static_cast<Square>(index);
    }

    // helper to split the fen
    inline std::vector<std::string> split(const std::string &str, char del) {
        std::vector<std::string> parts;
        std::string current;

        for (const char c: str) {
            if (c == del) {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }

        if (!current.empty()) {
            parts.push_back(current);
        }

        return parts;
    }

    // helper to get the castling hash index for the zobrist key
    inline int getCastleHashIndex(const U64 castle_mask) {
        bool wks = castle_mask & WHITE_OO_MASK;
        bool wqs = castle_mask & WHITE_OOO_MASK;
        bool bks = castle_mask & BLACK_OO_MASK;
        bool bqs = castle_mask & BLACK_OOO_MASK;
        return !wks + 2 * !wqs + 4 * !bks + 8 * !bqs;
    }

    // helper to represent the castle rights in the fen notation
    inline bool castleNotationHelper(const std::ostringstream &fen_stream) {
        const std::string fen = fen_stream.str();
        const std::string rights = fen.substr(fen.find(' ') + 1);
        return rights.find_first_of("kqKQ") != std::string::npos;
    }

    inline PieceType typeOfPromotion(const MoveFlags mf) {
        switch (mf) {
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

    inline bool isCapture(const Move &m) {
        const MoveFlags flag = m.flag();
        return flag == CAPTURE || flag == EN_PASSANT || (flag >= PC_KNIGHT && flag <= PC_QUEEN);
    }

    inline bool isPromotion(const Move &m) {
        const MoveFlags flag = m.flag();
        return flag >= PR_KNIGHT && flag <= PC_QUEEN;
    }

    // prints the move
    inline std::ostream &operator<<(std::ostream &os, const Move &m) {
        if (SQSTR[m.from()] == "a1" && SQSTR[m.to()] == "a1") {
            os << "NULL MOVE";
        } else {
            os << SQSTR[m.from()] << SQSTR[m.to()];
        }

        if(isPromotion(m)) {
            const PieceType pt = typeOfPromotion(m.flag());
            os << PIECE_STR[pt + 6];
        }

        return os;
    }

    // gets the opposite color
    constexpr Color operator~(Color c) {
        return static_cast<Color>(c ^ BLACK);
    }

    constexpr Piece makePiece(Color c, PieceType pt) {
        if (pt == NO_PIECE_TYPE) {
            return NO_PIECE;
        }
        return static_cast<Piece>(pt + 6 * c);
    }

    constexpr PieceType typeOf(Piece p) {
        return PIECE_TO_PIECE_TYPE[p];
    }

    constexpr Color colorOf(Piece p) {
        return p < 6 ? WHITE : BLACK;
    }

    inline Square &operator++(Square &s) {
        s = static_cast<Square>(static_cast<int>(s) + 1);
        return s;
    }

    constexpr Square operator+(Square s, Direction d) {
        return static_cast<Square>(static_cast<int>(s) + static_cast<int>(d));
    }

    constexpr Square operator-(Square s, Direction d) {
        return static_cast<Square>(static_cast<int>(s) - static_cast<int>(d));
    }

    inline Square &operator+=(Square &s, Direction d) {
        return s = s + d;
    }

    inline Square &operator-=(Square &s, Direction d) {
        return s = s - d;
    }

    constexpr Rank rankOf(Square s) {
        return static_cast<Rank>(s >> 3);
    }

    constexpr File fileOf(Square s) {
        return static_cast<File>(s & 0b111);
    }

    // gets the diagonal (a1 to h8) of the square
    constexpr int diagOf(Square s) {
        return 7 + rankOf(s) - fileOf(s);
    }

    // gets the anti-diagonal (h1 to a8) of the square
    constexpr int antiDiagOf(Square s) {
        return rankOf(s) + fileOf(s);
    }

    constexpr Rank relativeRank(Color c, Rank r) {
        return c == WHITE ? r : static_cast<Rank>(RANK_8 - r);
    }

    constexpr Direction relativeDir(Color c, Direction d) {
        return static_cast<Direction>(c == WHITE ? d : -d);
    }

    constexpr Square mirrorVertically(Color c, Square s) {
        return c == WHITE ? s : static_cast<Square>(s ^ 56);
    }

} // namespace Chess

#endif //MISC_H
