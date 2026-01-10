#pragma once

#include "../chess/misc.h"

using namespace chess;

namespace search {

enum Bound : uint8_t {
    NO_BOUND,
    LOWER_BOUND,
    UPPER_BOUND,
    EXACT_BOUND,
};

#pragma pack(push, 1)
class TTEntry {
  public:
    void store(U64 hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv);

    void refresh_age();

    int relative_age() const;

    uint8_t get_age() const;

    U64 get_hash() const {
        return hash;
    }

    uint8_t get_depth() const {
        return depth;
    }

    Move get_move() const {
        return Move(move);
    }

    Score get_score(int ply) const {
        if (!valid_score(score))
            return SCORE_NONE;
        if (is_win(score))
            return score - ply;
        if (is_loss(score))
            return score + ply;
        return score;
    }

    Score get_eval() const {
        return eval;
    }

    Bound get_bound() const {
        return Bound(agepvbound & 0x3);
    }

    bool get_tt_pv() {
        return agepvbound & 0x4;
    }

  private:
    uint16_t hash = 0;
    uint8_t depth = 0;
    uint16_t move = 0;
    Score score = SCORE_NONE;
    Score eval = SCORE_NONE;
    uint8_t agepvbound = NO_BOUND;
};
#pragma pack(pop)

constexpr int BUCKET_SIZE = 3;

struct TTBucket {
    TTEntry entries[BUCKET_SIZE];
    int16_t padding;
};

static_assert(sizeof(TTBucket) == 32, "TTBucket is not packed as expected!");

class TTable {
  public:
    explicit TTable(U64 size_mb);
    ~TTable();

    void init(U64 size_mb);
    void clear();

    void increment();

    TTEntry* lookup(U64 hash, bool* hit) const;

    int hashfull() const;

    void prefetch(U64 hash) const {
        __builtin_prefetch(&buckets[index(hash)]);
    }

    int get_age() const {
        return age;
    }

  private:
    uint8_t age;
    U64 bucket_size;
    TTBucket* buckets;

    size_t index(U64 hash) const {
        return (static_cast<unsigned __int128>(hash) * static_cast<unsigned __int128>(bucket_size)) >> 64;
    }
};

inline bool valid_tt_score(Score tt_score, Score score, Bound bound) {
    return (bound & (tt_score >= score ? LOWER_BOUND : UPPER_BOUND));
}

// Global Variable

extern TTable tt;

} // namespace search
