#include "tt.h"

namespace Astra
{
    TTable::TTable(U64 size_mb) : current_age(0), entries(nullptr)
    {
        init(size_mb);
    }

    TTable::~TTable() { delete[] entries; }

    void TTable::init(U64 size_mb)
    {
        current_age = 0;

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

    void TTable::clear() const
    {
        for (U64 i = 0; i < tt_size; i++)
            entries[i] = TTEntry();
    }

    bool TTable::lookup(TTEntry& entry, U64 hash) const
    {
        U64 idx = hash & mask;
        if (entries[idx].hash == hash)
        {
            entry = entries[idx];
            return true;
        }
        return false;
    }

    void TTable::store(U64 hash, Move move, Score score, int depth, Bound bound) const
    {
        U64 idx = hash & mask;

        if (!entries[idx].hash)
        {
            // save if no entry is present
            entries[idx] = TTEntry(hash, depth, move, score, bound);
            entries[idx].age = current_age;
        }
        else
        {
            if ((bound == EXACT_BOUND) || 
                (entries[idx].hash != hash) ||
                (entries[idx].age != current_age) || 
                (entries[idx].hash == hash && entries[idx].depth <= depth))
            {
                entries[idx] = TTEntry(hash, depth, move, score, bound);
                entries[idx].age = current_age;
            }
        }
    }

    void TTable::incrementAge()
    {
        current_age++;
        if (current_age == 255)
            current_age = 0;
    }

    void TTable::prefetch(U64 hash) const
    {
        __builtin_prefetch(&entries[hash % tt_size]);
    }

    int TTable::hashfull() const
    {
        int used = 0;
        for (U64 i = 0; i < 1000; i++)
            if (entries[i].hash)
                used++;

        return used;
    }

    Score scoreToTT(Score s, int ply)
    {
        if (s >= VALUE_TB_WIN_IN_MAX_PLY)
            return s + ply;
        if (s <= -VALUE_TB_WIN_IN_MAX_PLY)
            return s - ply;
        return s;
    }

    Score scoreFromTT(Score s, int ply)
    {
        if (s == VALUE_NONE)
            return VALUE_NONE;
        if (s >= VALUE_TB_WIN_IN_MAX_PLY)
            return s - ply;
        if (s <= -VALUE_TB_WIN_IN_MAX_PLY)
            return s + ply;
        return s;
    }

} // namespace Astra
