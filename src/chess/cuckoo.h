#pragma once

#include "misc.h"

namespace Chess::Cuckoo {

extern U64 keys[8192];
extern Move cuckoo_moves[8192];

inline int cuckooH1(U64 hash) {
    return hash & 0x1fff;
}

inline int cuckooH2(U64 hash) {
    return (hash >> 16) & 0x1fff;
}

void init();

} // namespace Chess::Cuckoo
