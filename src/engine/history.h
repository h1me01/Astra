#pragma once

#include "../chess/board.h"
#include "../chess/movegen.h"

#include "stack.h"

using namespace Chess;

namespace Engine {

constexpr size_t CORR_SIZE = 16384;
constexpr size_t PAWN_HIST_SIZE = 512;

int history_bonus(int depth);
int history_malus(int depth);

class History {
  public:
    // variable

    int16_t conth[2][NUM_PIECES + 1][NUM_SQUARES + 1][NUM_PIECES + 1][NUM_SQUARES + 1];

    // functions

    void clear();

    void update(const Board &board,       //
                const Move &best_move,    //
                const MoveList<> &quiets, //
                const MoveList<> &noisy,  //
                Stack *stack,             //
                int depth);

    void update_hh(Color c, const Move &move, int bonus);
    void update_nh(const Board &board, const Move &move, int bonus);
    void update_ph(const Board &board, const Move &move, int bonus);

    void update_conth(const Move &move, Stack *stack, int bonus);

    void update_mat_corr(const Board &board, Score raw_eval, Score real_score, int depth);
    void update_cont_corr(Score raw_eval, Score real_score, int depth, const Stack *stack);

    Move get_counter(const Move &move) const;

    int get_hh(Color stm, const Move &move) const;
    int get_nh(const Board &board, const Move &move) const;
    int get_qh(const Board &board, const Move &move, const Stack *stack) const;
    int get_ph(const Board &board, const Move &move) const;

    int get_mat_corr(const Board &board) const;
    int get_cont_corr(const Stack *stack) const;

  private:
    // variables

    Move counters[NUM_SQUARES][NUM_SQUARES]{};

    int16_t hh[NUM_COLORS][NUM_SQUARES][NUM_SQUARES];
    int16_t nh[NUM_PIECES][NUM_SQUARES][NUM_PIECE_TYPES + 1];

    int16_t ph[PAWN_HIST_SIZE][NUM_PIECES][NUM_SQUARES];

    int16_t pawn_corr[NUM_COLORS][CORR_SIZE];
    int16_t w_non_pawn_corr[NUM_COLORS][CORR_SIZE];
    int16_t b_non_pawn_corr[NUM_COLORS][CORR_SIZE];

    int16_t cont_corr[NUM_PIECES][NUM_SQUARES][NUM_PIECES][NUM_SQUARES];

    // functions

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

inline int History::get_hh(Color stm, const Move &move) const {
    assert(move);
    return hh[stm][move.from()][move.to()];
}

inline int History::get_nh(const Board &board, const Move &move) const {
    assert(move);

    Piece pc = board.piece_at(move.from());
    PieceType captured = move.is_ep() ? PAWN : piece_type(board.piece_at(move.to()));

    assert(valid_piece(pc));
    assert(captured != KING);
    assert(valid_piece_type(captured) || (move.type() >= PQ_KNIGHT && move.type() <= PQ_QUEEN));

    return nh[pc][move.to()][captured];
}

inline int History::get_qh(const Board &board, const Move &move, const Stack *stack) const {
    assert(move);

    Square to = move.to();
    Piece pc = board.piece_at(move.from());

    assert(valid_piece(pc));

    return get_hh(board.get_stm(), move) +       //
           get_ph(board, move) +                 //
           (int) (*(stack - 1)->conth)[pc][to] + //
           (int) (*(stack - 2)->conth)[pc][to] + //
           (int) (*(stack - 4)->conth)[pc][to];
}

inline int History::get_ph(const Board &board, const Move &move) const {
    assert(move);
    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));
    return ph[ph_idx(board.get_pawn_hash())][pc][move.to()];
}

inline int History::get_mat_corr(const Board &board) const {
    Color stm = board.get_stm();
    return pawn_corr[stm][corr_idx(board.get_pawn_hash())] +               //
           w_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(WHITE))] + //
           b_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(BLACK))];
}

inline int History::get_cont_corr(const Stack *stack) const {
    const Move prev_move = (stack - 1)->move;
    const Move pprev_move = (stack - 2)->move;

    if(prev_move && pprev_move)
        return cont_corr[(stack - 1)->moved_piece][prev_move.to()][(stack - 2)->moved_piece][pprev_move.to()];
    else
        return 0;
}

} // namespace Engine
