#include <iostream>
#include "misc.h"

namespace Chess
{
    void printBitboard(const U64 b)
    {
        for (int rank = 7; rank >= 0; --rank)
        {
            for (int file = 0; file < 8; ++file)
            {
                int s = rank * 8 + file;
                U64 mask = 1ULL << s;
                std::cout << ((b & mask) ? "1 " : "0 ");
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    Square squareFromString(std::string_view square_str)
    {
        int file = square_str[0] - 97;
        int rank = square_str[1] - 49;
        int index = rank * 8 + file;
        return static_cast<Square>(index);
    }

    std::vector<std::string> split(const std::string& str, char del)
    {
        std::vector<std::string> parts;
        std::string current;

        for (const char c : str)
        {
            if (c == del)
            {
                if (!current.empty())
                {
                    parts.push_back(current);
                    current.clear();
                }
            }
            else
                current += c;
        }

        if (!current.empty())
            parts.push_back(current);

        return parts;
    }

    std::ostream& operator<<(std::ostream& os, const Move& m)
    {
        if (SQSTR[m.from()] == "a1" && SQSTR[m.to()] == "a1")
            os << "NULL MOVE";
        else
            os << SQSTR[m.from()] << SQSTR[m.to()];

        if (isPromotion(m))
        {
            const PieceType pt = typeOfPromotion(m.flag());
            os << PIECE_STR[pt + 6];
        }

        return os;
    }

} // namespace Chess
