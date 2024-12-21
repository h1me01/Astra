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

    class TTEntry
    {
    public:
        int relativeAge();
        
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

        int getDepth() const { return depth; }

        uint8_t getAge() const { return age_pv_bound >> 3; }

        Bound getBound() const { return Bound(age_pv_bound & EXACT_BOUND); }

        Move getMove() const { return Move(move); }

        Score getEval() const { return eval; }

        bool wasPv() const { return age_pv_bound & 0x4; }

        bool isSame(U64 hash) const { return uint16_t(hash) == this->hash; }

        void store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv);

    private:
        uint16_t hash;
        uint8_t depth;
        int16_t move;
        uint8_t age_pv_bound;
        int16_t eval;
        Score score;
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

        TTBucket *getBucket(U64 hash) const;
    };

    extern TTable tt;

} // namespace Astra

#endif // TT_H
