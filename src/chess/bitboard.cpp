#include "bitboard.h"

namespace Chess
{
    int popCount(U64 b)
    {
        b = b - ((b >> 1) & 0x5555555555555555);
        b = (b & 0x3333333333333333) + ((b >> 2) & 0x3333333333333333);
        b = (b + (b >> 4)) & 0x0f0f0f0f0f0f0f0f;
        b = (b * 0x0101010101010101) >> 56;
        return int(b);
    }

    int sparsePopCount(U64 b)
    {
        int count = 0;
        while (b)
        {
            count++;
            b &= b - 1;
        }
        return count;
    }

    Square popLsb(U64 &b)
    {
        int n = lsb(b);
        b &= b - 1;
        return Square(n);
    }

    U64 reverse(U64 b)
    {
        b = ((b & 0x5555555555555555) << 1) | ((b >> 1) & 0x5555555555555555);
        b = ((b & 0x3333333333333333) << 2) | ((b >> 2) & 0x3333333333333333);
        b = ((b & 0x0f0f0f0f0f0f0f0f) << 4) | ((b >> 4) & 0x0f0f0f0f0f0f0f0f);
        b = ((b & 0x00ff00ff00ff00ff) << 8) | ((b >> 8) & 0x00ff00ff00ff00ff);
        return (b << 48) | ((b & 0xffff0000) << 16) | ((b >> 16) & 0xffff0000) | (b >> 48);
    }

} // namespace Chess
