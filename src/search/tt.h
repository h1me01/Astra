#ifndef TT_H
#define TT_H

#include "../chess/types.h"

using namespace Chess;

namespace Astra
{
    enum Bound
    {
        NO_BOUND = 0,
        LOWER_BOUND = 1,
        UPPER_BOUND = 2,
        EXACT_BOUND = 3
    };

    struct TTEntry
    {
        U64 hash = 0;
        uint8_t age = 0;
        uint8_t depth = 0;
        Move move = NO_MOVE;
        Score score = VALUE_NONE;
        Score eval = VALUE_NONE;
        Bound bound = NO_BOUND;

        Score getScore(int ply) const
        {
            if (score == VALUE_NONE)
                return VALUE_NONE;
            if (score >= VALUE_TB_WIN_IN_MAX_PLY)
                return score - ply;
            if (score <= -VALUE_TB_WIN_IN_MAX_PLY)
                return score + ply;
            return score;
        }

        void store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply);
    };

    class TTable
    {
    public:
        int num_workers = 1;

        explicit TTable(U64 size_mb);
        ~TTable();

        void init(U64 size_mb);
        void clear();

        TTEntry *lookup(U64 hash, bool *hit) const;

        void incrementAge()
        {
            age++;
            if (age == 255)
                age = 0;
        }

        void prefetch(U64 hash) const
        {
            __builtin_prefetch(&entries[hash & mask]);
        }

        int hashfull() const
        {
            int used = 0;
            for (U64 i = 0; i < 1000; i++)
                if (entries[i].hash)
                    used++;

            return used;
        }

        int getAge() const { return age; }

    private:
        uint8_t age;
        U64 tt_size{};
        U64 mask{};
        TTEntry *entries;
    };

    extern TTable tt;

} // namespace Astra

#endif // TT_H
