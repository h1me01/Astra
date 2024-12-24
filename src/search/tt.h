#ifndef TT_H
#define TT_H

#include "../chess/types.h"

using namespace Chess;

namespace Astra
{
    constexpr int PV_MASK = 0x4;

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
        uint8_t age_pv_bound;

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

        bool wasPv() const { return age_pv_bound & PV_MASK; }

        bool isSame(U64 hash) const { return uint16_t(hash) == this->hash; }

        bool isEmpty() const { return age_pv_bound == 0 && score == 0; }

        void store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv);

    private:
        uint16_t hash;
        uint8_t depth;
        uint16_t move;
        int16_t eval;
        int16_t score;
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

        TTEntry* lookup(U64 hash, bool& hit);

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
