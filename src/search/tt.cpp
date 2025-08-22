#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sys/mman.h>
#include <xmmintrin.h>
#endif

#include "tt.h"

namespace Search {

void *allocAlign(size_t size) {
#if defined(__linux__)
    constexpr size_t alignment = 2 * 1024 * 1024;
#else
    constexpr size_t alignment = 4096;
#endif
    size = ((size + alignment - 1) / alignment) * alignment;
    void *ptr = _mm_malloc(size, alignment);
#if defined(__linux__)
    madvise(ptr, size, MADV_HUGEPAGE);
#endif
    return ptr;
}

void freeAlign(void *ptr) {
    _mm_free(ptr);
}

// struct TTEntry
int TTEntry::relative_age() const {
    return (AGE_CYCLE + tt.get_age() - agepvbound) & AGE_MASK;
}

void TTEntry::store( //
    U64 hash,        //
    Move move,       //
    Score score,     //
    Score eval,      //
    Bound bound,     //
    int depth,       //
    int ply,         //
    bool pv          //
) {
    uint16_t hash16 = (uint16_t) hash;

    if(move || this->hash != hash16)
        this->move = move.raw();

    if(valid_score(score)) {
        if(is_win(score))
            score += ply;
        if(is_loss(score))
            score -= ply;
    }

    if(bound == EXACT_BOUND || this->hash != hash16 || depth + 4 + 2 * pv > this->depth) {
        this->hash = hash16;
        this->depth = depth;
        this->score = score;
        this->eval = eval;
        agepvbound = (uint8_t) (bound + (pv << 2)) | tt.get_age();
    } else if(this->depth >= 5 && bound != EXACT_BOUND) {
        this->depth--;
    }
}

// class TTable
TTable::TTable(U64 size_mb) : buckets(nullptr) {
    init(size_mb);
}

TTable::~TTable() {
    freeAlign(buckets);
}

void TTable::init(U64 size_mb) {
    if(buckets)
        freeAlign(buckets);

    U64 size_bytes = size_mb * 1024 * 1024;
    bucket_size = size_bytes / sizeof(TTBucket);

    buckets = (TTBucket *) allocAlign(size_bytes);

    clear();
}

void TTable::clear() {
    age = 0;

    const U64 chunk_size = bucket_size / num_workers;

    std::vector<std::thread> threads;
    threads.reserve(num_workers);

    for(int i = 0; i < num_workers; i++) {
        threads.emplace_back([this, i, chunk_size]() {
            for(U64 j = 0; j < chunk_size; ++j)
                buckets[i * chunk_size + j] = TTBucket{};
        });
    }

    const U64 cleared = chunk_size * num_workers;
    if(cleared < bucket_size)
        for(U64 i = cleared; i < bucket_size; ++i)
            buckets[i] = TTBucket{};

    for(auto &t : threads)
        t.join();
}

TTEntry *TTable::lookup(U64 hash, bool *hit) const {
    uint16_t hash16 = (uint16_t) hash;
    TTEntry *entries = buckets[index(hash)].entries;

    for(int i = 0; i < BUCKET_SIZE; i++) {
        if(entries[i].get_hash() == hash16 || entries[i].get_hash() == 0) {
            uint8_t agepvbound = (uint8_t) (tt.get_age() | (entries[i].get_agepvbound() & (AGE_STEP - 1)));
            entries[i].set_agepvbound(agepvbound);

            *hit = entries[i].get_hash() == hash16;
            return &entries[i];
        }
    }

    TTEntry *worst_entry = &entries[0];
    for(int i = 1; i < BUCKET_SIZE; i++) {
        int worst_value = worst_entry->get_depth() - worst_entry->relative_age();
        int entry_value = entries[i].get_depth() - entries[i].relative_age();

        if(entry_value < worst_value)
            worst_entry = &entries[i];
    }

    *hit = false;
    return worst_entry;
}

int TTable::hashfull() const {
    int used = 0;
    for(int i = 0; i < 1000; i++) {
        for(int j = 0; j < BUCKET_SIZE; j++) {
            TTEntry *entry = &buckets[i].entries[j];
            if(entry->get_age() == age && entry->get_hash() != 0)
                used++;
        }
    }

    return used / BUCKET_SIZE;
}

TTable tt(16);

} // namespace Search
