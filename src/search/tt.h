#ifndef TT_H
#define TT_H

#include "../chess/types.h"

using namespace Chess;

namespace Astra
{
    constexpr uint8_t TT_MAX_AGE = 1 << 5;

    enum Bound
    {
        NO_BOUND = 0,
        LOWER_BOUND = 1,
        UPPER_BOUND = 2,
        EXACT_BOUND = 3
    };

    struct TTEntry
    {
        uint16_t hash;
        uint8_t depth;
        uint16_t move;
        uint8_t age_pv_bound;
        int16_t eval;
        Score score;

        uint8_t getAge() const { return age_pv_bound >> 3; }
        int ageDistance();
        Bound getBound() const { return Bound(age_pv_bound & 0x3); }
        Move getMove() const { return Move(move); }
        bool wasPv() const { return age_pv_bound & 0x4; }
        bool empty() const { return !score && !age_pv_bound; }
        bool isSame(U64 hash) const { return uint16_t(hash) == this->hash; }

        void store(U64 hash, Move move, Score score, Score eval, int depth, Bound bound, bool pv);
    };

    constexpr int entries_per_bucket = 3;

    struct TTBucket
    {
        TTEntry entries[entries_per_bucket];
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

        bool lookup(TTEntry *&entry, U64 hash);

        void incrementAge();
        void prefetch(U64 hash) const;

        int hashfull() const;
        int getAge() const { return age; }

    private:
        uint8_t age;
        U64 bucket_size{};
        TTBucket *buckets;

        TTBucket* getBucket(U64 hash) const;
    };

    inline Score scoreToTT(Score s, int ply)
    {
        if (s >= VALUE_TB_WIN_IN_MAX_PLY)
            return s + ply;
        if (s <= -VALUE_TB_WIN_IN_MAX_PLY)
            return s - ply;
        return s;
    }

    inline Score scoreFromTT(Score s, int ply)
    {
        if (s == VALUE_NONE)
            return VALUE_NONE;
        if (s >= VALUE_TB_WIN_IN_MAX_PLY)
            return s - ply;
        if (s <= -VALUE_TB_WIN_IN_MAX_PLY)
            return s + ply;
        return s;
    }

    extern TTable tt;

} // namespace Astra

#endif // TT_H
