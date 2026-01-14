#pragma once

#include "../chess/board.h"
#include "../chess/movegen.h"

#include "stack.h"

using namespace chess;

namespace search {

inline int history_bonus(int d) {
    return std::min<int>(max_hist_bonus, hist_bonus_mult * d + hist_bonus_minus);
}

inline int history_malus(int d) {
    return std::min<int>(max_hist_malus, hist_malus_mult * d + hist_malus_minus);
}

class QuietHistory {
  public:
    QuietHistory() {
        clear();
    }

    void clear() {
        std::memset(data, 0, sizeof(data));
    }

    void update(Color c, Move move, int bonus);

    int get(Color c, Move move) const {
        assert(valid_color(c) && move);
        return data[c][move.from()][move.to()];
    }

  private:
    int16_t data[NUM_COLORS][NUM_SQUARES][NUM_SQUARES];
};

class NoisyHistory {
  public:
    NoisyHistory() {
        clear();
    }

    void clear() {
        std::memset(data, 0, sizeof(data));
    }

    void update(const Board& board, Move move, int bonus);

    int get(const Board& board, Move move) const {
        assert(move);

        Piece pc = board.piece_at(move.from());
        PieceType captured = piece_type(board.capture_piece(move));

        assert(valid_piece(pc));
        assert(captured != KING);
        assert(valid_piece_type(captured) || move.is_prom());

        return data[pc][move.to()][captured];
    }

  private:
    int16_t data[NUM_PIECES][NUM_SQUARES][NUM_PIECE_TYPES + 1];
};

class PawnHistory {
  public:
    PawnHistory() {
        clear();
    }

    void clear() {
        std::memset(data, 0, sizeof(data));
    }

    void update(const Board& board, Move move, int bonus);

    int get(const Board& board, Move move) const {
        assert(move);

        Piece pc = board.piece_at(move.from());
        assert(valid_piece(pc));

        return data[idx(board.pawn_hash())][pc][move.to()];
    }

  private:
    static constexpr size_t size = 8192;

  private:
    int16_t data[size][NUM_PIECES][NUM_SQUARES];

    int idx(U64 hash) const {
        return hash & (size - 1);
    }
};

class ContinuationHistory {
  public:
    ContinuationHistory() {
        clear();
    }

    void clear() {
        std::memset(data, 0, sizeof(data));
    }

    void update(Piece pc, Square to, int bonus, Stack* stack);

    PieceToContinuation* get() {
        return &data[0][0][NO_PIECE][0];
    }

    PieceToContinuation* get(bool in_check, bool is_cap, Piece pc, Square to) {
        assert(valid_piece(pc) && valid_sq(to));

        return &data[in_check][is_cap][pc][to];
    }

  private:
    int16_t data[2][2][NUM_PIECES + 1][NUM_SQUARES][NUM_PIECES + 1][NUM_SQUARES];
};

class CorrectionHistories {
  public:
    CorrectionHistories() {
        clear();
    }

    void clear() {
        std::memset(pawn, 0, sizeof(pawn));
        std::memset(minor_piece, 0, sizeof(minor_piece));
        std::memset(w_non_pawn, 0, sizeof(w_non_pawn));
        std::memset(b_non_pawn, 0, sizeof(b_non_pawn));
    }

    void update(const Board& board, int bonus);

    int get(const Board& board) const {
        Color stm = board.side_to_move();
        return pawn[stm][idx(board.pawn_hash())]                  //
               + minor_piece[stm][idx(board.minor_piece_hash())]  //
               + w_non_pawn[stm][idx(board.non_pawn_hash(WHITE))] //
               + b_non_pawn[stm][idx(board.non_pawn_hash(BLACK))];
    }

  private:
    static constexpr size_t size = 16384;

  private:
    int16_t pawn[NUM_COLORS][size];
    int16_t minor_piece[NUM_COLORS][size];
    int16_t w_non_pawn[NUM_COLORS][size];
    int16_t b_non_pawn[NUM_COLORS][size];

    int idx(U64 hash) const {
        return hash & (size - 1);
    }
};

class ContinuationCorrectionHistory {
  public:
    ContinuationCorrectionHistory() {
        clear();
    }

    void clear() {
        std::memset(data, 0, sizeof(data));
    }

    void update(const Board& board, int bonus, const Stack* stack);

    int get(const Board& board, const Stack* stack) const {
        Move m = (stack - 1)->move;
        if (!m || !(stack - 2)->move)
            return 0;

        Piece pc = board.piece_at(m.to());
        assert(valid_piece(pc));

        return (*(stack - 2)->cont_corr_hist)[pc][m.to()];
    }

    PieceToContinuation* get() {
        return &data[NO_PIECE][0];
    }

    PieceToContinuation* get(Piece pc, Square to) {
        return &data[pc][to];
    }

  private:
    int16_t data[NUM_PIECES + 1][NUM_SQUARES][NUM_PIECES + 1][NUM_SQUARES];
};

} // namespace search
