#include "tt.h"

namespace Astra
{
    TTable::TTable(const int size_mb) : current_age(0), entries(nullptr)
    {
        init(size_mb);
    }

    TTable::~TTable() { delete[] entries; }

    void TTable::init(int size_mb)
    {
        current_age = 0;

        if (entries != nullptr)
            delete[] entries;

        const U64 size_bytes = size_mb * 1024 * 1024;
        U64 max_entries = size_bytes / sizeof(TTEntry);

        tt_size = 1;
        while (tt_size <= max_entries)
            tt_size *= 2;

        tt_size /= 2;
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

        if (entries[idx].hash == 0)
        {
            // save if no entry is present
            entries[idx] = TTEntry(hash, depth, move, score, bound);
            entries[idx].age = current_age;
        }
        else
        {
            // store if age is different then current age
            bool first = entries[idx].getAge() != current_age;
            // store if exact bound
            bool second = bound == EXACT_BOUND;
            // store if not exact bound but depth is greater or equal
            bool third = bound != EXACT_BOUND && entries[idx].depth <= depth;
            // store if key is equal and depth is greater
            bool fourth = entries[idx].hash == hash && entries[idx].depth <= depth + 3;

            if (first || second || third || fourth)
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
        __builtin_prefetch(&entries[hash & mask]);
    }

    Score scoreToTT(Score s, int ply)
    {
        if (s >= VALUE_TB_WIN_IN_MAX_PLY)
            return s + ply;
        if (s <= VALUE_TB_LOSS_IN_MAX_PLY)
            return s - ply;
        return s;
    }

    Score scoreFromTT(Score s, int ply)
    {
        if (s == VALUE_NONE)
            return VALUE_NONE;
        if (s >= VALUE_TB_WIN_IN_MAX_PLY)
            return s - ply;
        if (s <= VALUE_TB_LOSS_IN_MAX_PLY)
            return s + ply;
        return s;
    }

} // namespace Astra
