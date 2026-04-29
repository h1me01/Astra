#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sys/mman.h>
#include <xmmintrin.h>
#endif

#include "threads.h"
#include "tt.h"

namespace astra::search {

// Helper

void* alloc_align(size_t size) {
#if defined(__linux__)
    constexpr size_t alignment = 2 * 1024 * 1024;
#else
    constexpr size_t alignment = 4096;
#endif
    size = ((size + alignment - 1) / alignment) * alignment;
    void* ptr = _mm_malloc(size, alignment);
#if defined(__linux__)
    madvise(ptr, size, MADV_HUGEPAGE);
#endif
    return ptr;
}

void free_align(void* ptr) { _mm_free(ptr); }

// Constants

constexpr int AGE_STEP = 0x8;
constexpr int AGE_CYCLE = 255 + AGE_STEP;
constexpr int AGE_MASK = 0xF8;

// TTEntry

void TTEntry::refresh_age() { agepvbound = static_cast<uint8_t>(tt.age() | (agepvbound & (AGE_STEP - 1))); }
int TTEntry::relative_age() const { return (AGE_CYCLE + tt.age() - agepvbound) & AGE_MASK; }
uint8_t TTEntry::age() const { return agepvbound & AGE_MASK; }

void TTEntry::store(Hash hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv) {
    uint16_t hash16 = static_cast<uint16_t>(hash);

    if (move || hash_ != hash16)
        move_ = move;

    if (valid_score(score)) {
        if (is_win(score))
            score += ply;
        if (is_loss(score))
            score -= ply;
    }

    if (bound == EXACT_BOUND || hash_ != hash16 || depth + 4 + 2 * pv > depth_) {
        hash_ = hash16;
        depth_ = depth;
        score_ = score;
        eval_ = eval;
        agepvbound = static_cast<uint8_t>(bound + (pv << 2)) | tt.age();
    }
}

// TTable

TTable::TTable(uint64_t size_mb)
    : buckets_(nullptr) {
    init(size_mb);
}

TTable::~TTable() { free_align(buckets_); }

void TTable::init(uint64_t size_mb) {
    if (buckets_)
        free_align(buckets_);

    uint64_t size_bytes = size_mb * 1024 * 1024;
    bucket_size_ = size_bytes / sizeof(TTBucket);

    buckets_ = static_cast<TTBucket*>(alloc_align(size_bytes));

    clear();
}

void TTable::clear() {
    age_ = 0;

    const int worker_count = std::max(1, thread_pool.size());
    const uint64_t chunk_size = bucket_size_ / worker_count;

    std::vector<std::thread> threads;
    threads.reserve(worker_count);

    for (int i = 0; i < worker_count; i++) {
        threads.emplace_back([this, i, chunk_size]() {
            for (uint64_t j = 0; j < chunk_size; ++j)
                buckets_[i * chunk_size + j] = TTBucket{};
        });
    }

    const uint64_t cleared = chunk_size * worker_count;
    if (cleared < bucket_size_)
        for (uint64_t i = cleared; i < bucket_size_; ++i)
            buckets_[i] = TTBucket{};

    for (auto& t : threads)
        t.join();
}

void TTable::increment() { age_ += AGE_STEP; }

TTEntry* TTable::lookup(Hash hash, bool* hit) const {
    uint16_t hash16 = static_cast<uint16_t>(hash);
    auto& entries = buckets_[index(hash)].entries;

    for (int i = 0; i < TTBucket::SIZE; i++) {
        uint16_t entry_hash = entries(i).hash();
        if (entry_hash == hash16 || !entry_hash) {
            entries(i).refresh_age();
            *hit = (entry_hash == hash16);
            return &entries(i);
        }
    }

    auto* replace = &entries(0);
    int min_value = replace->depth() - 4 * replace->relative_age();

    for (int i = 1; i < TTBucket::SIZE; i++) {
        int value = entries(i).depth() - 4 * entries(i).relative_age();
        if (value < min_value) {
            min_value = value;
            replace = &entries(i);
        }
    }

    *hit = false;
    return replace;
}

int TTable::hashfull() const {
    int used = 0;
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < TTBucket::SIZE; j++) {
            const auto& entry = buckets_[i].entries(j);
            if (entry.age() == age_ && entry.hash() != 0)
                used++;
        }
    }
    return used / TTBucket::SIZE;
}

// Global Variable

TTable tt(16);

} // namespace astra::search
