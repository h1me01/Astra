#ifndef TT_H
#define TT_H

#include "../chess/types.h"

using namespace Chess;

namespace Astra
{

    constexpr uint8_t NO_DEPTH = 255;

    constexpr int PV_MASK = 0x4;
    constexpr int BOUND_MASK = 0x3;
    constexpr int AGE_STEP = 0x8;
    constexpr int AGE_CYCLE = 255 + AGE_STEP;
    constexpr int AGE_MASK = 0xF8;

    enum Bound
    {
        NO_BOUND = 0,
        LOWER_BOUND = 1,
        UPPER_BOUND = 2,
        EXACT_BOUND = 3
    };

    struct TTEntry
    {
        uint16_t hash = 0;
        uint8_t depth = NO_DEPTH;
        uint8_t age_pv_bound = 0;
        Move move = NO_MOVE;
        int16_t eval = 0;
        int16_t score = 0;

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

        uint8_t getAge() const { return age_pv_bound >> 3; }

        Bound getBound() const { return Bound(age_pv_bound & EXACT_BOUND); }

        bool wasPv() const { return age_pv_bound & PV_MASK; }

        bool isSame(U64 hash) const { return uint16_t(hash) == this->hash; }

        bool isEmpty() const { return eval == VALUE_NONE; }

        void store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv);
    };

    constexpr int BUCKET_SIZE = 3;

    struct TTBucket
    {
        TTEntry entries[BUCKET_SIZE];
        int16_t padding;
    };

    class TTable
    {
    public:
        int num_workers = 1;

        explicit TTable(U64 size_mb);
        
        ~TTable();

        void init(U64 size_mb);
        void clear();

        TTEntry *lookup(U64 hash, bool &hit);

        void incrementAge()
        {
            age += AGE_STEP;
        }

        void prefetch(U64 hash) const
        {
            __builtin_prefetch(getBucket(hash));
        }

        int hashfull() const
        {
            int count = 0;
            for (int i = 0; i < 1000; i++)
                for (int j = 0; j < BUCKET_SIZE; j++)
                {
                    TTEntry *entry = &buckets[i].entries[j];
                    if (entry->getAge() == age && !entry->isEmpty())
                        count++;
                }

            return count / BUCKET_SIZE;
        }

        int getAge() const { return age; }

    private:
        uint8_t age;
        U64 bucket_size{};
        TTBucket *buckets;

        TTBucket *getBucket(U64 hash) const
        {
            return &buckets[((unsigned __int128)hash * (unsigned __int128)bucket_size) >> 64];
        }
    };

    extern TTable tt;

} // namespace Astra

#endif // TT_H
