#ifndef TT_H
#define TT_H

#include "../chess/types.h"

using namespace Chess;

namespace Tsukuyomi {

    enum Bound {
        NO_BOUND,
        UPPER_BOUND,
        LOWER_BOUND,
        EXACT_BOUND
    };

    struct TTEntry {
        U64 hash;
        int depth;
        Move move;
        Score score;
        Bound bound;

        TTEntry() : hash(0), depth(0), move(NO_MOVE), score(VALUE_NONE), bound(NO_BOUND) {}

        TTEntry(U64 hash, int depth, Move move, Score score, Bound bound) :
                hash(hash), depth(depth), move(move), score(score), bound(bound) {}
    };

    class TTable {
    public:
        explicit TTable(int size_mb);
        ~TTable();

        void init(int size_mb);
        void clear() const;

        bool lookup(TTEntry& entry, U64 hash) const;
        void store(U64 hash, Move move, Score score, int depth, Bound bound) const;

    private:
        U64 tt_size;
        U64 mask;
        TTEntry *entries;

    };

} // namespace Astra

#endif //TT_H
