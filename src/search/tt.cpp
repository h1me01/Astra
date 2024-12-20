#include "tt.h"

#include <thread>
#include <vector>
#include <cstring>

namespace Astra
{
    TTable tt(16);

    // helper
    using uint128 = unsigned __int128;

    void *alignedMalloc(U64 size)
    {
#if defined(__linux__)
        constexpr size_t align = 2 * 1024 * 1024;
#else
        constexpr size_t align = 4096;
#endif
        size = ((size + align - 1) / align) * align;
        void *result = _mm_malloc(size, align);
#if defined(__linux__)
        madvise(result, size, MADV_HUGEPAGE);
#endif
        return result;
    }

    void alignedFree(void *ptr)
    {
        _mm_free(ptr);
    }

    // TTEntry struct
    int TTEntry::ageDistance() 
    { 
        return (TT_MAX_AGE + tt.getAge() - getAge()) % TT_MAX_AGE; 
    }

    void TTEntry::store(U64 hash, Move move, Score score, Score eval, int depth, Bound bound, bool pv)
    {
        bool matches = isSame(hash);

        if (!matches || move.raw())
            this->move = move.raw();

        if (bound == EXACT_BOUND || !matches || depth + 4 > this->depth)
        {
            this->hash = uint16_t(hash);
            this->depth = uint8_t(depth);
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
        TTEntry *entries = getBucket(hash)->entries;

        for (int i = 0; i < entries_per_bucket; i++)
        {
            if (entries[i].isSame(hash))
            {
                ent = &entries[i];
                return !ent->empty();
            }
        }

        TTEntry *worst_entry = &entries[0];
        for (int i = 1; i < entries_per_bucket; i++)
            if ((entries[i].depth - 8 * entries[i].ageDistance()) < (worst_entry->depth - 8 * worst_entry->ageDistance()))
                worst_entry = &entries[i];

        ent = worst_entry;
        return false;
    }

    void TTable::incrementAge()
    {
        age = (age + 1) % TT_MAX_AGE;
    }

    void TTable::prefetch(U64 hash) const
    {
        __builtin_prefetch(getBucket(hash));
    }

    int TTable::hashfull() const
    {
        int count = 0;
        for (int i = 0; i < 1000; i++)
            for (int j = 0; j < entries_per_bucket; j++)
            {
                TTEntry *entry = &buckets[i].entries[j];
                if (entry->getAge() == age && !entry->empty())
                    count++;
            }

        return count / entries_per_bucket;
    }

    TTBucket *TTable::getBucket(U64 hash) const
    {
        uint64_t idx = (uint128(hash) * uint128(bucket_size)) >> 64;
        return &buckets[idx];
    }

} // namespace Astra
