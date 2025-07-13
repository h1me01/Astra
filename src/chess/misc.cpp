#include "misc.h"

namespace Chess {

void print_bb(const U64 b) {
    for(int rank = 7; rank >= 0; --rank) {
        for(int file = 0; file < 8; ++file) {
            U64 mask = 1ULL << (rank * 8 + file);
            std::cout << ((b & mask) ? "1 " : "0 ");
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

Square sq_from(std::string_view square_str) {
    int file = square_str[0] - 97;
    int rank = square_str[1] - 49;
    int index = rank * 8 + file;
    return Square(index);
}

std::vector<std::string> split(const std::string &str, char del) {
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

PieceType prom_type(const MoveType mt) {
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

std::ostream &operator<<(std::ostream &os, const Move &m) {
    if(!m)
        os << "NO MOVE";
    else if(m == NULL_MOVE)
        os << "NULL MOVE";
    else
        os << SQSTR[m.from()] << SQSTR[m.to()];

    if(m.is_prom()) {
        const PieceType pt = prom_type(m.type());
        os << PIECE_STR[pt + 6];
    }

    return os;
}

} // namespace Chess
