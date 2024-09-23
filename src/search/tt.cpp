#include <iostream>
#include <cassert>
#include "tt.h"

namespace Astra {

    TTable::TTable(const int size_mb) {
        init(size_mb);
    }

    TTable::~TTable() {
        delete[] entries;
    }

    void TTable::init(int size_mb) {
        const U64 size_bytes = size_mb * 1024 * 1024;
        U64 max_entries = size_bytes / sizeof(TTEntry);

        tt_size = 1;
        while (tt_size <= max_entries) {
            tt_size *= 2;
        }

        tt_size /= 2;
        mask = tt_size - 1;

        try {
            entries = new TTEntry[tt_size];
            clear();
        } catch (const std::bad_alloc &e) {
            std::cerr << "Failed to allocate transposition table" << std::endl;
            std::cerr << "Error: " << e.what() << std::endl;
            exit(1);
        }
    }

    void TTable::clear() const {
        for (int i = 0; i < tt_size; i++) {
            entries[i] = TTEntry();
        }
    }

    bool TTable::lookup(TTEntry &entry, U64 hash) const {
        U64 idx = hash & mask;
        if (entries[idx].hash == hash) {
            entry = entries[idx];
            return true;
        }
        return false;
    }

    void TTable::store(U64 hash, Move move, Score score, int depth, Bound bound) const {
        U64 idx = hash & mask;

        if(entries[idx].hash == 0) {
            entries[idx] = TTEntry(hash, depth, move, score, bound);
        } else {
            bool first = bound == EXACT_BOUND; // store if exact bound
            bool second = bound != EXACT_BOUND && entries[idx].depth <= depth; // store if not exact bound but depth is greater or equal
            bool third = entries[idx].hash == hash && entries[idx].depth <= depth + 3; // store if key is equal and depth is greater

            if (first || second || third) {
                entries[idx] = TTEntry(hash, depth, move, score, bound);
            }
        }
    }

    void TTable::prefetch(U64 hash) const {
        __builtin_prefetch(&entries[hash & mask]);
    }

} // namespace Astra
