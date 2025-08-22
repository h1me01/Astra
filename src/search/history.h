#pragma once

#include "../chess/board.h"

#include "stack.h"

using namespace Chess;

namespace Search {

constexpr size_t CORR_SIZE = 16384;

int history_bonus(int depth);

class History {
  public:
    void update(const Board &board,    //
                const Move &move,      //
                Stack *s,              //
                Move *q_moves, int qc, //
                Move *c_moves, int cc, //
                int depth);

    void update_hh(Color c, const Move &move, int bonus);
    void update_nh(const Board &board, const Move &move, int bonus);
    void update_conth(const Move &move, Stack *ss, int bonus);
    void update_matcorr(const Board &board, Score raw_eval, Score real_score, int depth);

    Move get_counter(Move move) const;

    int get_hh(Color stm, Move move) const;
    int get_nh(const Board &board, Move &move) const;
    int get_qh(const Board &board, const Stack *ss, Move &move) const;
    int get_matcorr(const Board &board) const;

    int16_t conth[2][NUM_PIECES + 1][NUM_SQUARES + 1][NUM_PIECES + 1][NUM_SQUARES + 1];

  private:
    Move counters[NUM_SQUARES][NUM_SQUARES];

    int16_t hh[NUM_COLORS][NUM_SQUARES][NUM_SQUARES];
    int16_t nh[NUM_PIECES][NUM_SQUARES][NUM_PIECE_TYPES + 1];

    int16_t pawn_corr[NUM_COLORS][CORR_SIZE];
    int16_t w_non_pawn_corr[NUM_COLORS][CORR_SIZE];
    int16_t b_non_pawn_corr[NUM_COLORS][CORR_SIZE];

    int corr_idx(U64 hash) const {
        return hash % CORR_SIZE;
    }
};

inline Move History::get_counter(Move prev_move) const {
    Square from = prev_move.from();
    Square to = prev_move.to();

    assert(valid_sq(to));
    assert(valid_sq(from));

    return counters[from][to];
}

inline int History::get_hh(Color stm, Move move) const {
    return hh[stm][move.from()][move.to()];
}

inline int History::get_nh(const Board &board, Move &move) const {
    PieceType captured = move.type() == EN_PASSANT ? PAWN : piece_type(board.piece_at(move.to()));
    Piece pc = board.piece_at(move.from());

    assert(valid_piece(pc));
    assert(captured != KING);
    assert(valid_piece_type(captured) || (move.type() >= PQ_KNIGHT && move.type() <= PQ_QUEEN));

    return nh[pc][move.to()][captured];
}

inline int History::get_qh(const Board &board, const Stack *ss, Move &move) const {
    Square from = move.from();
    Square to = move.to();
    Piece pc = board.piece_at(from);

    assert(valid_sq(to));
    assert(valid_sq(from));
    assert(valid_piece(pc));

    return get_hh(board.get_stm(), move) +    //
           (int) (*(ss - 1)->conth)[pc][to] + //
           (int) (*(ss - 2)->conth)[pc][to] + //
           (int) (*(ss - 4)->conth)[pc][to];
}

inline int History::get_matcorr(const Board &board) const {
    Color stm = board.get_stm();

    return pawn_corr[stm][corr_idx(board.get_pawn_hash())] +               //
           w_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(WHITE))] + //
           b_non_pawn_corr[stm][corr_idx(board.get_nonpawn_hash(BLACK))];
}

} // namespace Search
