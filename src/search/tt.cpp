#include "tt.h"

#include <thread>
#include <vector>
#include <cstring>

namespace Astra
{
    TTable tt(16);

    // helper
    void *alignedMalloc(U64 size)
    {
#if defined(__linux__)
        constexpr size_t alignment = 2 * 1024 * 1024;
#else
        constexpr size_t alignment = 4096;
#endif
        size = ((size + alignment - 1) / alignment) * alignment;
        void *result = _mm_malloc(size, alignment);
#if defined(__linux__)
        madvise(result, size, MADV_HUGEPAGE);
#endif
        return result;
    }

    void alignedFree(void *ptr)
    {
        _mm_free(ptr);
    }

    // constants
    constexpr int AGE_STEP = 0x8;
    constexpr int AGE_CYCLE = 255 + AGE_STEP;
    constexpr int AGE_MASK = 0xF8;

    // TTEntry struct
    int TTEntry::relativeAge()
    {
        return (AGE_CYCLE + tt.getAge() - age_pv_bound) & AGE_MASK;
    }

    void TTEntry::store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv)
    {
        if (!isSame(hash) || move != NO_MOVE)
            this->move = move.raw();

        if (score != VALUE_NONE)
        {
            if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                score += ply;
            if (score <= -VALUE_TB_WIN_IN_MAX_PLY)
                score -= ply;
        }

        if (bound == EXACT_BOUND || !isSame(hash) || depth + 4 > getDepth())
        {
            this->hash = uint16_t(hash);
            this->depth = depth - 2;
            this->age_pv_bound = (tt.getAge() << 3) | (pv << 2) | bound;
            this->eval = eval;
            this->score = score;
        }
    }

    // TTable class
    TTable::TTable(U64 size_mb) : buckets(nullptr)
    {
        init(size_mb);
    }

    TTable::~TTable() { alignedFree(buckets); }

    void TTable::init(U64 size_mb)
    {
        if (buckets)
            alignedFree(buckets);

        U64 size_bytes = size_mb * 1024 * 1024;
        bucket_size = size_bytes / sizeof(TTBucket);

        buckets = (TTBucket *)alignedMalloc(size_bytes);
        clear();
    }

    void TTable::clear()
    {
        age = 0;

        const U64 chunk_size = bucket_size / num_workers;

        std::vector<std::thread> threads;
        threads.reserve(num_workers);

        for (int i = 0; i < num_workers; i++)
            threads.emplace_back([this, i, chunk_size]()
                                 { memset(&buckets[i * chunk_size], 0, chunk_size * sizeof(TTBucket)); });

        const U64 cleared = chunk_size * num_workers;
        if (cleared < bucket_size)
            memset(&buckets[cleared], 0, (bucket_size - cleared) * sizeof(TTBucket));

        for (auto &t : threads)
            t.join();
    }

    bool TTable::lookup(TTEntry *&ent, U64 hash)
    {
        TTEntry *bucket = getBucket(hash)->entries;

        for (int i = 0; i < BUCKET_SIZE; i++)
            if (bucket[i].isSame(hash))
            {
                ent = &bucket[i];
                return bool(ent->getDepth());
            }

        TTEntry *worst_entry = &bucket[0];
        for (int i = 1; i < BUCKET_SIZE; i++)
            if ((bucket[i].getDepth() - bucket[i].relativeAge() / 2) < (worst_entry->getDepth() - worst_entry->relativeAge() / 2))
                worst_entry = &bucket[i];

        ent = worst_entry;
        return false;
    }

    void TTable::incrementAge()
    {
        age += AGE_STEP;
    }

    void TTable::prefetch(U64 hash) const
    {
        __builtin_prefetch(getBucket(hash));
    }

    int TTable::hashfull() const
    {
        int count = 0;
        for (int i = 0; i < 1000; i++)
            for (int j = 0; j < BUCKET_SIZE; j++)
            {
                TTEntry *entry = &buckets[i].entries[j];
                if (entry->getAge() == age && bool(entry->getDepth()))
                    count++;
            }

        return count / BUCKET_SIZE;
    }

    TTBucket *TTable::getBucket(U64 hash) const
    {
        return &buckets[((unsigned __int128)hash * (unsigned __int128)bucket_size) >> 64];
    }

} // namespace Astra
