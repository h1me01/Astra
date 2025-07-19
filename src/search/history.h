#pragma once

#include "../chess/board.h"
#include "stack.h"

using namespace Chess;

namespace Astra {

constexpr size_t PAWN_HIST_SIZE = 512;
constexpr size_t CORR_SIZE = 16384;

int history_bonus(int depth);
int history_malus(int depth);

class History {
  private:
    Move counters[NUM_SQUARES][NUM_SQUARES];

    int16_t hh[NUM_COLORS][NUM_SQUARES][NUM_SQUARES]{};
    int16_t ch[NUM_PIECES][NUM_SQUARES][NUM_PIECE_TYPES]{};

    int16_t ph[PAWN_HIST_SIZE][NUM_PIECES][NUM_SQUARES]{};

    int16_t cont_corr[NUM_PIECES][NUM_SQUARES][NUM_PIECES][NUM_SQUARES]{};
    int16_t pawn_corr[NUM_COLORS][CORR_SIZE]{};
    int16_t w_non_pawn_corr[NUM_COLORS][CORR_SIZE]{};
    int16_t b_non_pawn_corr[NUM_COLORS][CORR_SIZE]{};

    void update_ch(const Board &board, Move &move, int bonus);

  public:
    int16_t conth[2][NUM_PIECES + 1][NUM_SQUARES + 1][NUM_PIECES + 1][NUM_SQUARES + 1]{};

    void update(const Board &board, Move &move, Stack *ss, Move *q_moves, int qc, Move *c_moves, int cc, int depth);

    void update_qh(Color c, Move move, int bonus);
    void update_ph(const Board &board, Move &move, int bonus);
    void update_conth(Move &move, Stack *ss, int bonus);
    void update_matcorr(const Board &board, Score raw_eval, Score real_score, int depth);
    void update_contcorr(Score raw_eval, Score real_score, int depth, const Stack *ss);

    Move get_counter(Move move) const;

    int get_hh(Color stm, Move move) const;
    int get_qh(const Board &board, const Stack *ss, Move &move) const;
    int get_ch(const Board &board, Move &move) const;
    int get_ph(const Board &board, Move &move) const;
    int get_matcorr(const Board &board) const;
    int get_contcorr(const Stack *ss) const;

    int ph_idx(U64 hash) const {
        return hash % PAWN_HIST_SIZE;
    }

    int corr_idx(U64 hash) const {
        return hash % CORR_SIZE;
    }
};

inline int History::get_hh(Color stm, Move move) const {
    return hh[stm][move.from()][move.to()];
}

inline int History::get_qh(const Board &board, const Stack *ss, Move &move) const {
    Square from = move.from();
    Square to = move.to();
    Piece pc = board.piece_at(from);

    assert(valid_sq(to));
    assert(valid_sq(from));
    assert(valid_piece(pc));

    return get_hh(board.get_stm(), move) +    //
           get_ph(board, move) +              //
           (int) (*(ss - 1)->conth)[pc][to] + //
           (int) (*(ss - 2)->conth)[pc][to] + //
           (int) (*(ss - 4)->conth)[pc][to];
}

inline int History::get_ch(const Board &board, Move &move) const {
    PieceType captured = move.type() == EN_PASSANT ? PAWN : piece_type(board.piece_at(move.to()));
    Piece pc = board.piece_at(move.from());

    assert(valid_piece(pc));
    assert(valid_piece_type(captured) && captured != KING);

    return ch[pc][move.to()][captured];
}

inline int History::get_ph(const Board &board, Move &move) const {
    Piece pc = board.piece_at(move.from());
    assert(valid_piece(pc));
    assert(valid_sq(move.to()));
    return ph[ph_idx(board.get_pawnhash())][pc][move.to()];
}

inline Move History::get_counter(Move prev_move) const {
    Square from = prev_move.from();
    Square to = prev_move.to();

    assert(valid_sq(to));
    assert(valid_sq(from));

    return counters[from][to];
}

inline int History::get_matcorr(const Board &board) const {
    Color stm = board.get_stm();

    return pawn_corr[stm][corr_idx(board.get_pawnhash())] +               //
           w_non_pawn_corr[stm][corr_idx(board.get_nonpawnhash(WHITE))] + //
           b_non_pawn_corr[stm][corr_idx(board.get_nonpawnhash(BLACK))];
}

inline int History::get_contcorr(const Stack *ss) const {
    const Move prev_move = (ss - 1)->move;
    const Move pprev_move = (ss - 2)->move;

    Piece prev_p = (ss - 1)->moved_piece;
    Piece pprev_p = (ss - 2)->moved_piece;

    if(!prev_move || !pprev_move)
        return 0;

    return cont_corr[prev_p][prev_move.to()][pprev_p][pprev_move.to()];
}

} // namespace Astra
