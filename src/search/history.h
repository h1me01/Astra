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

template <int Div>
void apply_bonus(int16_t& v, int bonus) {
    v += bonus - v * std::abs(bonus) / Div;
}

} // namespace

class QuietHistory {
    static constexpr int BONUS_DIV = 16384;

  public:
    QuietHistory() { clear(); }

    void clear() { data_.fill(0); }

    void update(Color c, Move move, int bonus) {
        assert(move);
        assert(is_valid(c));
        apply_bonus<BONUS_DIV>(data_(c, move.from(), move.to()), bonus);
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
    static constexpr int BONUS_DIV = 16384;

  public:
    NoisyHistory() { clear(); }

    void clear() { data_.fill(0); }

    void update(const Board& board, Move move, int bonus) {
        assert(move);

        Piece pc = board.piece_at(move.from());
        PieceType captured = type_of(board.capture_piece(move));

        assert(is_valid(pc));
        assert(is_valid(captured) || move.is_prom());

        apply_bonus<BONUS_DIV>(data_(pc, move.to(), captured), bonus);
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
    static constexpr int BONUS_DIV = 16384;

  public:
    PawnHistory() { clear(); }

    void clear() { data_.fill(0); }

    void update(const Board& board, Move move, int bonus) {
        assert(move);

        Piece pc = board.piece_at(move.from());
        assert(is_valid(pc));

        apply_bonus<BONUS_DIV>(data_(idx(board.pawn_hash()), pc, move.to()), bonus);
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
    static constexpr int BONUS_DIV = 16384;

  public:
    ContinuationHistory() { clear(); }

    void clear() { data_.fill(PieceToContinuation{}); }

    void update(Piece pc, Square to, int bonus, Stack* stack) {
        assert(is_valid(to));
        assert(is_valid(pc));

        for (int i : {1, 2, 4, 6})
            if ((stack - i)->move)
                apply_bonus<BONUS_DIV>((*(stack - i)->cont_hist)(pc, to), bonus);
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
    static constexpr int BONUS_DIV = 1024;

  public:
    CorrectionHistories() { clear(); }

    void clear() { data_.fill(0); }

    void update(const Board& board, int bonus) {
        int i = 0;
        for (auto hash : hashes(board))
            apply_bonus<BONUS_DIV>(data_(i++, board.side_to_move(), idx(hash)), bonus);
    }

    int get(const Board& board) const {
        const auto hashes = this->hashes(board);

        int i = 0, value = 0;
        for (auto weight : {p_corr_weight, m_corr_weight, np_corr_weight, np_corr_weight}) {
            value += weight * data_(i, board.side_to_move(), idx(hashes(i)));
            ++i;
        }

        return value;
    }

  private:
    NDArray<int16_t, 4, NUM_COLORS, SIZE> data_;

    int idx(Hash hash) const { return hash & (SIZE - 1); }

    NDArray<Hash, 4> hashes(const Board& board) const {
        return NDArray<Hash, 4>{
            board.pawn_hash(), board.minor_piece_hash(), board.non_pawn_hash(WHITE), board.non_pawn_hash(BLACK)
        };
    }
};

class ContinuationCorrectionHistory {
    static constexpr int BONUS_DIV = 1024;

  public:
    ContinuationCorrectionHistory() { clear(); }

    void clear() { data_.fill(PieceToContinuation{}); }

    void update(const Board& board, int bonus, const Stack* stack) {
        Move m = (stack - 1)->move;
        if (!m)
            return;

        Piece pc = board.piece_at(m.to());
        assert(is_valid(pc));

        for (auto i : {2, 4})
            if ((stack - i)->move)
                apply_bonus<BONUS_DIV>((*(stack - i)->cont_corr_hist)(pc, m.to()), bonus);
    }

    int get(const Board& board, const Stack* stack) const {
        Move m = (stack - 1)->move;
        if (!m)
            return 0;

        Piece pc = board.piece_at(m.to());
        assert(is_valid(pc));

        int value = 0;
        if ((stack - 2)->move)
            value += cont_corr_weight2 * (*(stack - 2)->cont_corr_hist)(pc, m.to());
        if ((stack - 4)->move)
            value += cont_corr_weight4 * (*(stack - 4)->cont_corr_hist)(pc, m.to());

        return value;
    }

    PieceToContinuation* get() { return &data_(NO_PIECE, 0); }
    PieceToContinuation* get(Piece pc, Square to) { return &data_(pc, to); }

  private:
    NDArray<PieceToContinuation, NUM_PIECES + 1, NUM_SQUARES> data_;
};

} // namespace astra::search
