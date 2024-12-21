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
        U64 hash;
        uint8_t depth;
        uint8_t age;
        Move move;
        Score score;
        Score eval;
        Bound bound;

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
    };

    class TTable
    {
    public:
        int num_workers = 1;

        explicit TTable(U64 size_mb);
        ~TTable();

        void init(U64 size_mb);
        void clear();

        TTEntry *lookup(U64 hash, bool &hit) const;
        void store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply);

        void incrementAge();
        void prefetch(U64 hash) const;

        int hashfull() const;

    private:
        uint8_t age;
        U64 tt_size{};
        U64 mask{};
        TTEntry *entries;
    };

    extern TTable tt;

} // namespace Astra

#endif // TT_H