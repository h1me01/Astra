#ifndef CHESS_CUCKOO_H
#define CHESS_CUCKOO_H

#include "misc.h"

namespace Chess
{

    extern U64 cuckoo[8192];
    extern Move cuckoo_moves[8192];

    inline int cuckooH1(U64 hash)
    {
        return hash & 0x1fff;
    }

    inline int cuckooH2(U64 hash)
    {
        return (hash >> 16) & 0x1fff;
    }

    void initCuckoo();

} // namespace Chess

#endif // CHESS_CUCKOO_H
