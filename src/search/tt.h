#pragma once

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

    constexpr int AGE_STEP = 0x8;
    constexpr int AGE_CYCLE = 255 + AGE_STEP;
    constexpr int AGE_MASK = 0xF8;

#pragma pack(push, 1)
    struct TTEntry
    {
        uint16_t hash = 0;
        uint8_t depth = 0;
        uint16_t move = 0;
        Score score = VALUE_NONE;
        Score eval = VALUE_NONE;
        uint8_t age_pv_bound = NO_BOUND;

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

        Move getMove() const { return Move(move); }
        Bound getBound() const { return Bound(age_pv_bound & 0x3); }
        uint8_t getAge() const { return age_pv_bound & AGE_MASK; }
        bool getTTPv() { return age_pv_bound & 0x4; }

        void store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv);
    };
#pragma pack(pop)

    constexpr int BUCKET_SIZE = 3;

    struct TTBucket
    {
        TTEntry entries[BUCKET_SIZE];
        int16_t padding;
    };

    static_assert(sizeof(TTBucket) == 32, "TTBucket is not packed as expected!");

    class TTable
    {
    public:
        int num_workers = 1;

        explicit TTable(U64 size_mb);
        ~TTable();

        void init(U64 size_mb);
        void clear();

        TTEntry *lookup(U64 hash, bool *hit) const;

        size_t index(U64 hash) const
        {
            return ((unsigned __int128)hash * (unsigned __int128)bucket_size) >> 64;
        }

        void prefetch(U64 hash) const
        {
            __builtin_prefetch(&buckets[index(hash)]);
        }

        int hashfull() const
        {
            int used = 0;
            for (int i = 0; i < 1000; i++)
                for (int j = 0; j < BUCKET_SIZE; j++)
                {
                    TTEntry *entry = &buckets[i].entries[j];
                    if (entry->getAge() == age && entry->depth)
                        used++;
                }

            return used / BUCKET_SIZE;
        }

        void incrementAge() { age += AGE_STEP; }

        int getAge() const { return age; }

    private:
        uint8_t age;
        U64 bucket_size{};
        TTBucket *buckets;
    };

    extern TTable tt;

} // namespace Astra
