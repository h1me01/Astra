#pragma once

#include "../chess/types.h"
#include "types.h"

namespace astra::search {

enum class Bound : uint8_t { NONE, LOWER, UPPER, EXACT };

#pragma pack(push, 1)
class TTEntry {
  public:
    void store(Hash hash, Move move, Score score, Score eval, Bound bound, int depth, int ply, bool pv);
    void refresh_age();

    int relative_age() const;
    uint8_t age() const;
    uint16_t hash() const { return hash_; }
    uint8_t depth() const { return depth_; }
    Move move() const { return move_; }

    Score score(int ply) const {
        if (!is_valid(score_))
            return SCORE_NONE;
        if (is_win(score_))
            return score_ - ply;
        if (is_loss(score_))
            return score_ + ply;
        return score_;
    }

    Score eval() const { return eval_; }
    Bound bound() const { return Bound(agepvbound & 0x3); }
    bool tt_pv() const { return agepvbound & 0x4; }

  private:
    uint16_t hash_ = 0;
    uint8_t depth_ = 0;
    Move move_ = Move::none();
    int16_t score_ = SCORE_NONE;
    int16_t eval_ = SCORE_NONE;
    uint8_t agepvbound = 0;
};
#pragma pack(pop)

struct TTBucket {
    static constexpr int SIZE = 3;

    NDArray<TTEntry, SIZE> entries;
    int16_t padding;
};

static_assert(sizeof(TTBucket) == 32, "TTBucket is not packed as expected!");

class TTable {
  public:
    explicit TTable(uint64_t size_mb);
    ~TTable();

    void init(uint64_t size_mb);
    void clear();
    void increment();
    TTEntry* lookup(Hash hash, bool* hit) const;
    int hashfull() const;
    void prefetch(Hash hash) const { __builtin_prefetch(&buckets_[index(hash)]); }
    int age() const { return age_; }

  private:
    uint8_t age_;
    uint64_t bucket_size_;
    TTBucket* buckets_;

    size_t index(Hash hash) const {
        return (static_cast<unsigned __int128>(hash) * static_cast<unsigned __int128>(bucket_size_)) >> 64;
    }
};

inline bool valid_tt_score(Score tt_score, Score score, Bound bound) {
    if (bound == Bound::LOWER)
        return tt_score >= score;
    if (bound == Bound::UPPER)
        return tt_score < score;
    return bound == Bound::EXACT;
}

extern TTable tt;

} // namespace astra::search
