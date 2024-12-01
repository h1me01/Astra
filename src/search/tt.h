#ifndef TT_H
#define TT_H

#include "../chess/types.h"

using namespace Chess;

namespace Astra
{
    enum Bound
    {
        NO_BOUND,
        UPPER_BOUND,
        LOWER_BOUND,
        EXACT_BOUND
    };

    struct TTEntry
    {
        U64 hash;
        uint8_t depth;
        uint8_t age;
        Move move;
        Score score;
        Bound bound;

        TTEntry() : hash(0), depth(0), age(0), move(NO_MOVE), score(VALUE_NONE), bound(NO_BOUND) { }

        TTEntry(U64 hash, int depth, Move move, Score score, Bound bound) :
            hash(hash), depth(depth), move(move), score(score), bound(bound) { }

        uint8_t getAge() const { return age;}
        void setAge(const uint8_t age) { this->age = age; }
    };

    class TTable
    {
    public:
        explicit TTable(U64 size_mb);
        ~TTable();

        void init(U64 size_mb);
        void clear() const;

        bool lookup(TTEntry& entry, U64 hash) const;
        void store(U64 hash, Move move, Score score, int depth, Bound bound) const;

        void incrementAge();
        void prefetch(U64 hash) const;

        int hashfull() const;
    private:
        uint8_t current_age;
        U64 tt_size{};
        U64 mask{};
        TTEntry* entries;
    };

    Score scoreToTT(Score s, int ply);

    Score scoreFromTT(Score s, int ply);

} // namespace Astra

#endif //TT_H
