#include <cstring>
#include <thread>
#include <vector>

#include "tt.h"

namespace Search {

// TTEntry

void TTEntry::store(U64 hash, Move move, Score score, Bound bound, int depth, int ply) {
    if(valid_score(score)) {
        if(is_win(score))
            score += ply;
        if(is_loss(score))
            score = -ply;
    }

    if(bound == EXACT_BOUND || this->hash != hash || this->age != tt.get_age() || depth >= this->depth) {
        this->hash = hash;
        this->age = tt.get_age();
        this->depth = depth;
        this->move = move;
        this->score = score;
        this->bound = bound;
    }
}

// TTable

TTable::TTable(U64 size_mb) : entries(nullptr) {
    init(size_mb);
}

TTable::~TTable() {
    delete[] entries;
}

void TTable::init(U64 size_mb) {
    if(entries != nullptr)
        delete[] entries;

    U64 size_bytes = size_mb * 1024 * 1024;
    U64 max_entries = size_bytes / sizeof(TTEntry);

    // find the next power of 2
    tt_size = 1ULL << (63 - __builtin_clzll(max_entries));
    mask = tt_size - 1;

    entries = new TTEntry[tt_size];
    clear();
}

void TTable::clear() {
    age = 0;
    for(U64 i = 0; i < tt_size; ++i)
        entries[i] = TTEntry{};
}

TTEntry *TTable::lookup(U64 hash, bool *hit) const {
    U64 idx = hash & mask;
    *hit = (entries[idx].hash == hash);
    return &entries[idx];
}

TTable tt(16);

} // namespace Search
