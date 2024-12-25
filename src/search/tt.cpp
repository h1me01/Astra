#include "tt.h"

#include <thread>
#include <vector>
#include <cstring>

namespace Astra
{
    // struct TTEntry

    void TTEntry::store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply)
    {
        uint32_t hash32 = uint32_t(hash >> 32);

        if (score != VALUE_NONE)
        {
            if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                score += ply;
            if (score <= -VALUE_TB_WIN_IN_MAX_PLY)
                score = -ply;
        }

        if (bound == EXACT_BOUND || this->hash != hash32 || this->age != tt.getAge() || depth >= this->depth)
        {
            this->hash = hash32;
            this->age = tt.getAge();
            this->depth = depth;
            this->move = move;
            this->score = score;
            this->eval = eval;
            this->bound = bound;
        }
    }

    // class TTable
    TTable::TTable(U64 size_mb) : entries(nullptr)
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
                                         entries[i * chunk_size + j] = TTEntry{}; });

        const U64 cleared = chunk_size * num_workers;
        if (cleared < tt_size)
            for (U64 i = cleared; i < tt_size; ++i)
                entries[i] = TTEntry{};

        for (auto &t : threads)
            t.join();
    }

    TTEntry *TTable::lookup(U64 hash, bool *hit) const
    {
        uint32_t hash32 = uint32_t(hash >> 32);

        U64 idx = hash32 & mask;
        if (entries[idx].hash == hash32)
        {
            *hit = true;
            return &entries[idx];
        }

        *hit = false;
        return &entries[idx];
    }

    TTable tt(16);

} // namespace Astra
