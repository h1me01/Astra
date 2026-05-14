#pragma once

#include <algorithm>
#include <cstring>

#include "../chess/board.h"
#include "../chess/movegen.h"
#include "../ndarray.h"
#include "tune_params.h"
#include "types.h"

namespace astra::search {

namespace {

void update_history(int16_t& v, int bonus) { v += bonus - v * std::abs(bonus) / 16384; }
void update_correction(int16_t& v, int bonus) { v += bonus - v * std::abs(bonus) / 1024; }

} // namespace

inline int history_bonus(int d) { return std::min<int>(hist_bonus_max, hist_bonus_mult * d + hist_bonus_bias); }
inline int history_malus(int d) { return std::min<int>(hist_malus_max, hist_malus_mult * d + hist_malus_bias); }

class QuietHistory {
  public:
    QuietHistory() { clear(); }

    void clear() { data_.fill(0); }

    void update(Color c, Move move, int bonus) {
        assert(move);
        assert(is_valid(c));
        update_history(data_(c, move.from(), move.to()), bonus);
    }

    int get(Color c, Move move) const {
        assert(move);
        assert(is_valid(c));
        return data_(c, move.from(), move.to());
    }

  private:
    NDArray<int16_t, NUM_COLORS, NUM_SQUARES, NUM_SQUARES> data_;
};

class NoisyHistory {
  public:
    NoisyHistory() { clear(); }

    void clear() { data_.fill(0); }

    void update(const Board& board, Move move, int bonus) {
        assert(move);

        Piece pc = board.piece_at(move.from());
        PieceType captured = type_of(board.capture_piece(move));

        assert(is_valid(pc));
        assert(is_valid(captured) || move.is_prom());

        update_history(data_(pc, move.to(), captured), bonus);
    }

    int get(const Board& board, Move move) const {
        assert(move);

        Piece pc = board.piece_at(move.from());
        PieceType captured = type_of(board.capture_piece(move));

        assert(is_valid(pc));
        assert(captured != KING);
        assert(is_valid(captured) || move.is_prom());

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

    void update(const Board& board, Move move, int bonus) {
        assert(move);

        Piece pc = board.piece_at(move.from());
        assert(is_valid(pc));

        update_history(data_(idx(board.pawn_hash()), pc, move.to()), bonus);
    }

    int get(const Board& board, Move move) const {
        assert(move);

        Piece pc = board.piece_at(move.from());
        assert(is_valid(pc));

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

    void update(Piece pc, Square to, int bonus, Stack* stack) {
        assert(is_valid(to));
        assert(is_valid(pc));

        for (int i : {1, 2, 4, 6})
            if ((stack - i)->move)
                update_history((*(stack - i)->cont_hist)(pc, to), bonus);
    }

    PieceToContinuation* get() { return &data_(0, 0, NO_PIECE, 0); }
    PieceToContinuation* get(bool in_check, bool is_cap, Piece pc, Square to) {
        assert(is_valid(to));
        assert(is_valid(pc));
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

    void update(const Board& board, int bonus) {
        Color stm = board.side_to_move();

        update_correction(pawn_(stm, idx(board.pawn_hash())), bonus);
        update_correction(minor_piece_(stm, idx(board.minor_piece_hash())), bonus);
        update_correction(w_non_pawn_(stm, idx(board.non_pawn_hash(WHITE))), bonus);
        update_correction(b_non_pawn_(stm, idx(board.non_pawn_hash(BLACK))), bonus);
    }

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

    void update(const Board& board, int bonus, const Stack* stack) {
        Move m = (stack - 1)->move;
        if (!m || !(stack - 2)->move)
            return;

        Piece pc = board.piece_at(m.to());
        assert(is_valid(pc));

        update_correction((*(stack - 2)->cont_corr_hist)(pc, m.to()), bonus);
    }

    int get(const Board& board, const Stack* stack) const {
        Move m = (stack - 1)->move;
        if (!m || !(stack - 2)->move)
            return 0;

        Piece pc = board.piece_at(m.to());
        assert(is_valid(pc));

        return cont_corr_weight * (*(stack - 2)->cont_corr_hist)(pc, m.to());
    }

    PieceToContinuation* get() { return &data_(NO_PIECE, 0); }
    PieceToContinuation* get(Piece pc, Square to) { return &data_(pc, to); }

  private:
    NDArray<PieceToContinuation, NUM_PIECES + 1, NUM_SQUARES> data_;
};

} // namespace astra::search
