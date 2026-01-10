#pragma once

namespace chess {

struct StateInfo {
    Piece captured = NO_PIECE;
    Square ep_sq = NO_SQUARE;
    CastlingRights castling_rights;

    int fmr_counter = 0; // fifty move rule
    int plies_from_null = 0;
    int repetition = 0;

    U64 hash = 0;
    U64 pawn_hash = 0;
    U64 minor_piece_hash = 0;
    U64 non_pawn_hash[NUM_COLORS] = {0};

    U64 checkers = 0;
    U64 pinners[NUM_COLORS] = {0};
    U64 blockers[NUM_COLORS] = {0};
    U64 check_squares[NUM_PIECE_TYPES] = {0};
};

class StateInfoList {
  public:
    StateInfoList()
        : idx(0) {}

    StateInfo& operator[](int i) {
        assert(i >= 0 && i < max_size);
        return list[i];
    }

    const StateInfo& operator[](int i) const {
        assert(i >= 0 && i < max_size);
        return list[i];
    }

    StateInfo& increment() {
        assert(idx < max_size);
        idx++;
        list[idx] = list[idx - 1];
        return list[idx];
    }

    void decrement() {
        assert(idx > 0);
        idx--;
    }

    void clear() {
        idx = 0;
        list[0] = StateInfo();
    }

    void reset() {
        list[0] = list[idx];
        idx = 0;
    }

    StateInfo& back() {
        assert(idx >= 0 && idx < max_size);
        return list[idx];
    }

    const StateInfo& back() const {
        assert(idx >= 0 && idx < max_size);
        return list[idx];
    }

    int size() const {
        return idx + 1;
    }

  private:
    static constexpr int max_size = 1024;

  private:
    int idx;
    std::array<StateInfo, max_size> list;
};

} // namespace chess
