#pragma once

#include <algorithm>
#include <cstring>

#include "../chess/board.h"
#include "../chess/movegen.h"
#include "../ndarray.h"
#include "tune_params.h"
#include "types.h"

namespace astra::search {

inline int history_bonus(int d) { return std::min<int>(max_hist_bonus, hist_bonus_mult * d + hist_bonus_minus); }
inline int history_malus(int d) { return std::min<int>(max_hist_malus, hist_malus_mult * d + hist_malus_minus); }

class QuietHistory {
  public:
    QuietHistory() { clear(); }

    void clear() { data_.fill(0); }
    void update(Color c, Move move, int bonus);

    int get(Color c, Move move) const {
        assert(move);
        assert(valid_color(c));
        return data_(c, move.from(), move.to());
    }

  private:
    NDArray<int16_t, NUM_COLORS, NUM_SQUARES, NUM_SQUARES> data_;
};

class NoisyHistory {
  public:
    NoisyHistory() { clear(); }

    void clear() { data_.fill(0); }
    void update(const Board& board, Move move, int bonus);

    int get(const Board& board, Move move) const {
        assert(move);

        Piece pc = board.piece_at(move.from());
        PieceType captured = piece_type(board.capture_piece(move));

        assert(valid_piece(pc));
        assert(captured != KING);
        assert(valid_piece_type(captured) || move.is_prom());

        return data_(pc, move.to(), captured);
    }

  private:
    NDArray<int16_t, NUM_PIECES, NUM_SQUARES, NUM_PIECE_TYPES + 1> data_;
};

class PawnHistory {
    static constexpr size_t SIZE = 8192;

  public:
    PawnHistory() { clear(); }

    void clear() { data_.fill(0); }
    void update(const Board& board, Move move, int bonus);

    int get(const Board& board, Move move) const {
        assert(move);

        Piece pc = board.piece_at(move.from());
        assert(valid_piece(pc));

        return data_(idx(board.pawn_hash()), pc, move.to());
    }

  private:
    NDArray<int16_t, SIZE, NUM_PIECES, NUM_SQUARES> data_;

    int idx(Hash hash) const { return hash & (SIZE - 1); }
};

class ContinuationHistory {
  public:
    ContinuationHistory() { clear(); }

    void clear() { data_.fill(PieceToContinuation{}); }
    void update(Piece pc, Square to, int bonus, Stack* stack);

    PieceToContinuation* get() { return &data_(0, 0, NO_PIECE, 0); }
    PieceToContinuation* get(bool in_check, bool is_cap, Piece pc, Square to) {
        assert(valid_sq(to));
        assert(valid_piece(pc));
        return &data_(in_check, is_cap, pc, to);
    }

  private:
    NDArray<PieceToContinuation, 2, 2, NUM_PIECES + 1, NUM_SQUARES> data_;
};

class CorrectionHistories {
    static constexpr size_t SIZE = 16384;

  public:
    CorrectionHistories() { clear(); }

    void clear() {
        pawn_.fill(0);
        minor_piece_.fill(0);
        w_non_pawn_.fill(0);
        b_non_pawn_.fill(0);
    }

    void update(const Board& board, int bonus);

    int get(const Board& board) const {
        Color stm = board.side_to_move();
        return p_corr_weight * pawn_(stm, idx(board.pawn_hash()))                   //
               + m_corr_weight * minor_piece_(stm, idx(board.minor_piece_hash()))   //
               + np_corr_weight * w_non_pawn_(stm, idx(board.non_pawn_hash(WHITE))) //
               + np_corr_weight * b_non_pawn_(stm, idx(board.non_pawn_hash(BLACK)));
    }

  private:
    NDArray<int16_t, NUM_COLORS, SIZE> pawn_;
    NDArray<int16_t, NUM_COLORS, SIZE> minor_piece_;
    NDArray<int16_t, NUM_COLORS, SIZE> w_non_pawn_;
    NDArray<int16_t, NUM_COLORS, SIZE> b_non_pawn_;

    int idx(Hash hash) const { return hash & (SIZE - 1); }
};

class ContinuationCorrectionHistory {
  public:
    ContinuationCorrectionHistory() { clear(); }

    void clear() { data_.fill(PieceToContinuation{}); }
    void update(const Board& board, int bonus, const Stack* stack);

    int get(const Board& board, const Stack* stack) const {
        Move m = (stack - 1)->move;
        if (!m || !(stack - 2)->move)
            return 0;

        Piece pc = board.piece_at(m.to());
        assert(valid_piece(pc));

        return cont_corr_weight * (*(stack - 2)->cont_corr_hist)(pc, m.to());
    }

    PieceToContinuation* get() { return &data_(NO_PIECE, 0); }
    PieceToContinuation* get(Piece pc, Square to) { return &data_(pc, to); }

  private:
    NDArray<PieceToContinuation, NUM_PIECES + 1, NUM_SQUARES> data_;
};

} // namespace astra::search
