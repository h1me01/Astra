#pragma once

#include "types.h"

namespace astra::cuckoo {

inline int h1(Hash hash) { return hash & 0x1fff; }
inline int h2(Hash hash) { return (hash >> 16) & 0x1fff; }

Hash hash(const int idx);
Move move(const int idx);

void init();

} // namespace astra::cuckoo
