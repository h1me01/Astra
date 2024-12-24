#include "tt.h"

#include <thread>
#include <vector>
#include <cstring>
#include <emmintrin.h>

#if defined(__linux__)
#include <sys/mman.h>
#endif

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

    // TTEntry struct
    void TTEntry::store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv)
    {
        if (!isSame(hash) || (move != NO_MOVE && move != NULL_MOVE))
            this->move = move;

        if (score != VALUE_NONE)
        {
            if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                score += ply;
            if (score <= -VALUE_TB_WIN_IN_MAX_PLY)
                score -= ply;
        }

        if (bound == EXACT_BOUND || !isSame(hash) || this->depth == NO_DEPTH || depth + 4 + 2 * pv > this->depth)
        {
            this->hash = uint16_t(hash);
            this->depth = uint8_t(depth);
            this->age_pv_bound = (uint8_t)(bound + (pv << 2)) | tt.getAge();
            this->eval = eval;
            this->score = score;
        }
    }

    // TTable class
    TTable::TTable(U64 size_mb) : buckets(nullptr) { init(size_mb); }

    TTable::~TTable() { alignedFree(buckets); }

    void TTable::init(U64 size_mb)
    {
        if (buckets)
            alignedFree(buckets);

        U64 size_bytes = size_mb * 1024 * 1024;
        num_buckets = size_bytes / sizeof(TTBucket);

        buckets = (TTBucket *)alignedMalloc(size_bytes);
        clear();
    }

    void TTable::clear()
    {
        age = 0;

        const U64 chunk_size = num_buckets / num_workers;

        std::vector<std::thread> threads;
        threads.reserve(num_workers);

        for (int i = 0; i < num_workers; i++)
            threads.emplace_back([this, i, chunk_size]()
                                 {
                                     for (U64 j = i * chunk_size; j < (i + 1) * chunk_size; ++j)
                                         buckets[j] = TTBucket(); });

        const U64 cleared = chunk_size * num_workers;
        if (cleared < num_buckets)
        {
            for (U64 i = cleared; i < num_buckets; ++i)
                buckets[i] = TTBucket();
        }

        for (auto &t : threads)
            t.join();
    }

    TTEntry *TTable::lookup(U64 hash, bool *hit)
    {
        TTEntry *bucket = getBucket(hash)->entries;

        for (int i = 0; i < BUCKET_SIZE; i++)
            if (bucket[i].isSame(hash) || bucket[i].depth == NO_DEPTH)
            {
                bucket[i].age_pv_bound = (uint8_t)(tt.getAge() | (bucket[i].age_pv_bound & (AGE_STEP - 1)));
                *hit = bucket[i].isSame(hash);
                return &bucket[i];
            }

        // get worst entry for replacement
        TTEntry *worst_entry = &bucket[0];
        for (int i = 1; i < BUCKET_SIZE; i++)
        {
            int worst_age = worst_entry->depth;
            worst_age -= (AGE_CYCLE + tt.getAge() - worst_entry->age_pv_bound) & AGE_MASK;

            int current_age = bucket[i].depth;
            current_age -= (AGE_CYCLE + tt.getAge() - bucket[i].age_pv_bound) & AGE_MASK;

            if (current_age < worst_age || (current_age == NO_DEPTH && worst_age != NO_DEPTH))
                worst_entry = &bucket[i];
        }

        *hit = false;
        return worst_entry;
    }

} // namespace Astra
