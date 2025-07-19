#pragma once

#include "../chess/misc.h"

using namespace Chess;

namespace Astra {

enum Bound : uint8_t { //
    NO_BOUND = 0,
    LOWER_BOUND = 1,
    UPPER_BOUND = 2,
    EXACT_BOUND = 3
};

constexpr int AGE_STEP = 0x8;
constexpr int AGE_CYCLE = 255 + AGE_STEP;
constexpr int AGE_MASK = 0xF8;

#pragma pack(push, 1)
class TTEntry {
  public:
    int relative_age() const;

    void store(      //
        U64 hash,    //
        Move move,   //
        Score score, //
        Score eval,  //
        Bound bound, //
        int depth,   //
        int ply,     //
        bool pv      //
    );

    void set_agepvbound(uint8_t age_pv_bound) {
        this->agepvbound = age_pv_bound;
    }

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
        if(score == VALUE_NONE)
            return VALUE_NONE;
        if(score >= VALUE_TB_WIN_IN_MAX_PLY)
            return score - ply;
        if(score <= VALUE_TB_LOSS_IN_MAX_PLY)
            return score + ply;
        return score;
    }

    Score get_eval() const {
        return eval;
    }

    uint8_t get_agepvbound() const {
        return agepvbound;
    }

    Bound get_bound() const {
        return Bound(agepvbound & 0x3);
    }

    uint8_t get_age() const {
        return agepvbound & AGE_MASK;
    }

    bool get_tt_pv() {
        return agepvbound & 0x4;
    }

  private:
    uint16_t hash = 0;
    uint8_t depth = 0;
    uint16_t move = 0;
    Score score = VALUE_NONE;
    Score eval = VALUE_NONE;
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

    int hashfull() const;
    TTEntry *lookup(U64 hash, bool *hit) const;

    size_t index(U64 hash) const {
        return ((unsigned __int128) hash * (unsigned __int128) bucket_size) >> 64;
    }

    void prefetch(U64 hash) const {
        __builtin_prefetch(&buckets[index(hash)]);
    }

    void increment() {
        age += AGE_STEP;
    }

    void set_num_workers(int num_workers) {
        this->num_workers = num_workers;
    }

    int get_age() const {
        return age;
    }

  private:
    uint8_t age;
    U64 bucket_size;
    TTBucket *buckets;

    int num_workers = 1;
};

inline bool valid_tt_score(Score tt_score, Score score, Bound bound) {
    return (bound & (tt_score >= score ? LOWER_BOUND : UPPER_BOUND));
}

extern TTable tt;

} // namespace Astra
