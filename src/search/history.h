#pragma once

#include "../chess/board.h"
#include "../chess/movegen.h"

#include "stack.h"

using namespace chess;

namespace search {

class History {
  public:
    static int history_bonus(int depth);
    static int history_malus(int depth);

  public:
    int16_t cont_hist[2][2][NUM_PIECES + 1][NUM_SQUARES][NUM_PIECES + 1][NUM_SQUARES];

    void clear();

    void update(const Board &board,           //
                const Move &best_move,        //
                const MoveList<Move> &quiets, //
                const MoveList<Move> &noisy,  //
                Stack *stack,                 //
                int depth);

    void update_quiet_hist(Color c, const Move &move, int bonus);
    void update_noisy_hist(const Board &board, const Move &move, int bonus);
    void update_pawn_hist(const Board &board, const Move &move, int bonus);
    void update_cont_hist(const Piece pc, const Square to, Stack *stack, int bonus);

    void update_mat_corr(const Board &board, Score raw_eval, Score real_score, int depth);
    void update_cont_corr(Score raw_eval, Score real_score, int depth, const Stack *stack);

    Move get_counter(const Move &move) const;

    int get_heuristic_hist(Color stm, const Move &move) const;
    int get_noisy_hist(const Board &board, const Move &move) const;
    int get_quiet_hist(const Board &board, const Move &move, const Stack *stack) const;
    int get_pawn_hist(const Board &board, const Move &move) const;

    int get_corr_value(const Board &board, const Stack *stack) const;

  private:
    static constexpr size_t CORR_SIZE = 16384;
    static constexpr size_t PAWN_HIST_SIZE = 8192;

  private:
    Move counters[NUM_SQUARES][NUM_SQUARES];

    int16_t heuristic_hist[NUM_COLORS][NUM_SQUARES][NUM_SQUARES];
    int16_t noisy_hist[NUM_PIECES][NUM_SQUARES][NUM_PIECE_TYPES + 1];
    int16_t pawn_hist[PAWN_HIST_SIZE][NUM_PIECES][NUM_SQUARES];

    int16_t pawn_corr[NUM_COLORS][CORR_SIZE];
    int16_t w_non_pawn_corr[NUM_COLORS][CORR_SIZE];
    int16_t b_non_pawn_corr[NUM_COLORS][CORR_SIZE];
    int16_t cont_corr[NUM_PIECES][NUM_SQUARES][NUM_PIECES][NUM_SQUARES];

    int ph_idx(U64 hash) const {
        return hash % PAWN_HIST_SIZE;
    }

    int corr_idx(U64 hash) const {
        return hash % CORR_SIZE;
    }
};

inline Move History::get_counter(const Move &prev_move) const {
    assert(prev_move);
    return counters[prev_move.from()][prev_move.to()];
}

inline int History::get_heuristic_hist(Color stm, const Move &move) const {
    assert(move);
    return heuristic_hist[stm][move.from()][move.to()];
}

inline int History::get_noisy_hist(const Board &board, const Move &move) const {
    assert(move);

    Piece pc = board.piece_at(move.from());
    PieceType captured = piece_type(board.captured_piece(move));

    assert(valid_piece(pc));
    assert(captured != KING);
    assert(valid_piece_type(captured) || move.is_prom());

    return noisy_hist[pc][move.to()][captured];
}

inline int History::get_quiet_hist(const Board &board, const Move &move, const Stack *stack) const {
    assert(move);

    Square to = move.to();
    Piece pc = board.piece_at(move.from());

    assert(valid_piece(pc));

    return get_heuristic_hist(board.side_to_move(), move)        //
           + get_pawn_hist(board, move)                          //
           + static_cast<int>((*(stack - 1)->cont_hist)[pc][to]) //
           + static_cast<int>((*(stack - 2)->cont_hist)[pc][to]) //
           + static_cast<int>((*(stack - 4)->cont_hist)[pc][to]);
}

inline int History::get_pawn_hist(const Board &board, const Move &move) const {
    assert(move);
    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));
    return pawn_hist[ph_idx(board.pawn_hash())][pc][move.to()];
}

inline int History::get_corr_value(const Board &board, const Stack *stack) const {
    Color stm = board.side_to_move();
    Move prev_move = (stack - 1)->move;
    Move pprev_move = (stack - 2)->move;

    int value = pawn_corr[stm][corr_idx(board.pawn_hash())]                  //
                + w_non_pawn_corr[stm][corr_idx(board.non_pawn_hash(WHITE))] //
                + b_non_pawn_corr[stm][corr_idx(board.non_pawn_hash(BLACK))];

    if(prev_move && pprev_move)
        value += cont_corr[(stack - 1)->moved_piece][prev_move.to()][(stack - 2)->moved_piece][pprev_move.to()];

    return value / 256;
}

} // namespace search
