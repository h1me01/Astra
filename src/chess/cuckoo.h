#pragma once

#include "misc.h"

namespace chess::cuckoo {

inline int cuckoo_h1(U64 hash) {
    return hash & 0x1fff;
}

inline int cuckoo_h2(U64 hash) {
    return (hash >> 16) & 0x1fff;
}

U64 get_hash(const int idx);
Move get_move(const int idx);

void init();

} // namespace chess::cuckoo
