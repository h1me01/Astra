#include "tt.h"

#include <thread>
#include <vector>
#include <cstring>

namespace Astra
{
    TTable::TTable(U64 size_mb) : age(0), entries(nullptr)
    {
        init(size_mb);
    }

    TTable::~TTable() { delete[] entries; }

    void TTable::init(U64 size_mb)
    {
        if (entries != nullptr)
            delete[] entries;

        U64 size_bytes = size_mb * 1024 * 1024;
        U64 max_entries = size_bytes / sizeof(TTEntry);

        // find the next power of 2
        tt_size = 1ULL << (63 - __builtin_clzll(max_entries));
        mask = tt_size - 1;

        entries = new TTEntry[tt_size];
        clear();
    }

    void TTable::clear()
    {
        age = 0;

        const U64 chunk_size = tt_size / num_workers;

        std::vector<std::thread> threads;
        threads.reserve(num_workers);

        for (int i = 0; i < num_workers; i++)
            threads.emplace_back([this, i, chunk_size]()
                                 {
                                     for (U64 j = 0; j < chunk_size; ++j)
                                         entries[i * chunk_size + j] = TTEntry{}; 
                                 });

        const U64 cleared = chunk_size * num_workers;
        if (cleared < tt_size)
            for (U64 i = cleared; i < tt_size; ++i)
                entries[i] = TTEntry{}; 

        for (auto &t : threads)
            t.join();
    }

    TTEntry *TTable::lookup(U64 hash, bool &hit) const
    {
        U64 idx = hash & mask;
        if (entries[idx].hash == hash)
        {
            hit = true;
            return &entries[idx];
        }

        hit = false;
        return &entries[idx];
    }

    void TTable::store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply)
    {
        U64 idx = hash & mask;

        if (score != VALUE_NONE)
        {
            if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                score += ply;
            if (score <= -VALUE_TB_WIN_IN_MAX_PLY)
                score = -ply;
        }

        if (entries[idx].hash == 0 ||                                   // save if no entry is present
            bound == EXACT_BOUND ||                                     // save if exact bound
            entries[idx].hash != hash ||                                // save if hash is different
            entries[idx].age != age ||                                  // save if age is different
            (entries[idx].hash == hash && entries[idx].depth <= depth)) // save if depth is greater or equal
        {
            entries[idx].hash = hash;
            entries[idx].depth = depth;
            entries[idx].move = move;
            entries[idx].score = score;
            entries[idx].eval = eval;
            entries[idx].bound = bound;
            entries[idx].age = age;
        }
    }

    void TTable::incrementAge()
    {
        age++;
        if (age == 255)
            age = 0;
    }

    void TTable::prefetch(U64 hash) const
    {
        __builtin_prefetch(&entries[hash & mask]);
    }

    int TTable::hashfull() const
    {
        int used = 0;
        for (U64 i = 0; i < 1000; i++)
            if (entries[i].hash)
                used++;

        return used;
    }

    TTable tt(16);

} // namespace Astra