#pragma once

#include <algorithm>
#include <format>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "chess/types.h"

template <>
struct std::formatter<astra::Square> : std::formatter<std::string_view> {
    auto format(astra::Square sq, std::format_context& ctx) const {
        if (sq == astra::NO_SQUARE)
            return std::formatter<std::string_view>::format("no square", ctx);
        char buf[2] = {char('a' + astra::sq_file(sq)), char('1' + astra::sq_rank(sq))};
        return std::formatter<std::string_view>::format(std::string_view(buf, 2), ctx);
    }
};

template <>
struct std::formatter<astra::Piece> : std::formatter<char> {
    auto format(astra::Piece pc, std::format_context& ctx) const {
        return std::formatter<char>::format(astra::PIECE_STR[pc], ctx);
    }
};

template <>
struct std::formatter<astra::Move> : std::formatter<std::string> {
    auto format(astra::Move move, std::format_context& ctx) const {
        std::string s;
        if (move.is_none())
            s = "none";
        else if (move.is_null())
            s = "null";
        else {
            s = std::format("{}{}", move.from(), move.to());
            if (move.is_prom())
                s += std::format("{}", static_cast<astra::Piece>(move.prom_type() + 6));
        }
        return std::formatter<std::string>::format(s, ctx);
    }
};

namespace astra {

template <typename... Args>
void print(std::format_string<Args...> fmt, Args&&... args) {
    std::cout << std::format(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void println(std::format_string<Args...> fmt, Args&&... args) {
    print(fmt, std::forward<Args>(args)...);
    std::cout << '\n' << std::flush;
}

inline void print_bb(const Bitboard b) {
    for (int r = 7; r >= 0; --r) {
        for (int f = 0; f < 8; ++f) {
            Bitboard mask = 1ULL << (r * 8 + f);
            print("{}", (b & mask) ? "1 " : "0 ");
        }
        println("");
    }
    println("");
}

inline std::string to_lower(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return lower_str;
}

inline std::vector<std::string> split(const std::string& str, char del) {
    std::vector<std::string> parts;

    std::string current;
    for (const char c : str) {
        if (c == del) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }

    if (!current.empty())
        parts.push_back(current);

    return parts;
}

inline std::ostream& operator<<(std::ostream& os, Square sq) { return os << std::format("{}", sq); }
inline std::ostream& operator<<(std::ostream& os, Piece pc) { return os << std::format("{}", pc); }
inline std::ostream& operator<<(std::ostream& os, Move move) { return os << std::format("{}", move); }

} // namespace astra
