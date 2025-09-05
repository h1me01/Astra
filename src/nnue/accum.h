#pragma once

#include <array>
#include <cassert>
#include <memory>

#include "nnue.h"

using namespace Chess;

namespace NNUE {

struct DirtyPiece {
    Piece pc = NO_PIECE;
    Square from = NO_SQUARE;
    Square to = NO_SQUARE;

    DirtyPiece() {}
    DirtyPiece(Piece pc, Square from, Square to) : pc(pc), from(from), to(to) {}
};

class Accum {
  public:
    void update(Accum &prev, Color view);

    void reset() {
        initialized[WHITE] = m_needs_refresh[WHITE] = false;
        initialized[BLACK] = m_needs_refresh[BLACK] = false;
        num_dpcs = 0;
    }

    void put_piece(Piece pc, Square to, Square wksq, Square bksq) {
        assert(num_dpcs < 4);
        this->wksq = wksq;
        this->bksq = bksq;
        dpcs[num_dpcs++] = DirtyPiece(pc, NO_SQUARE, to);
    }

    void remove_piece(Piece pc, Square from, Square wksq, Square bksq) {
        assert(num_dpcs < 4);
        this->wksq = wksq;
        this->bksq = bksq;
        dpcs[num_dpcs++] = DirtyPiece(pc, from, NO_SQUARE);
    }

    void move_piece(Piece pc, Square from, Square to, Square wksq, Square bksq) {
        assert(num_dpcs < 4);
        this->wksq = wksq;
        this->bksq = bksq;
        dpcs[num_dpcs++] = DirtyPiece(pc, from, to);
    }

    void mark_as_initialized(Color view) {
        this->initialized[view] = true;
    }

    void set_refresh(Color view) {
        this->m_needs_refresh[view] = true;
    }

    bool is_initialized(Color view) const {
        return initialized[view];
    }

    bool needs_refresh(Color view) const {
        return m_needs_refresh[view];
    }

    int16_t *get_data(Color view) {
        return data[view];
    }

    int16_t *get_data(Color view) const {
        return const_cast<int16_t *>(data[view]);
    }

  private:
    Square wksq, bksq;

    bool initialized[NUM_COLORS] = {false, false};
    bool m_needs_refresh[NUM_COLORS] = {false, false};

    alignas(ALIGNMENT) int16_t data[NUM_COLORS][FT_SIZE];

    int num_dpcs = 0;
    // an accumulator can update at max only 4 pieces per move:
    // such case might be pawn captures piece on promotion rank:
    //  1. remove captured piece
    //  2. move pawn to target square
    //  3. add promotion piece to target square
    //  4. remove pawn from target square
    DirtyPiece dpcs[4];
};

// idea from koivisto
class AccumEntry {
  public:
    void reset() {
        memset(piece_bb, 0, sizeof(piece_bb));
        nnue.init_accum(acc);
    }

    void set_piecebb(Color c, PieceType pt, U64 bb) {
        piece_bb[c][pt] = bb;
    }

    U64 get_piecebb(Color c, PieceType pt) const {
        return piece_bb[c][pt];
    }

    Accum &get_accum() {
        return acc;
    }

  private:
    Accum acc;
    U64 piece_bb[NUM_COLORS][NUM_PIECE_TYPES];
};

class AccumTable {
  public:
    void reset();
    void refresh(Board &board, Color view);

  private:
    AccumEntry entries[NUM_COLORS][2 * BUCKET_SIZE];
};

class AccumList {
  public:
    AccumList() : idx(0), board(nullptr), data{} {}

    AccumList &copy(const AccumList &other, Board *new_board) {
        if(this != &other) {
            idx = other.idx;
            board = new_board;
            data = other.data;
            accum_table = std::make_unique<AccumTable>(*other.accum_table);
        }
        return *this;
    }

    void set_board(Board *new_board) {
        board = new_board;
    }

    void pop() {
        if(idx > 0)
            idx--;
    }

    void push() {
        assert(idx < MAX_PLY);
        idx++;
        data[idx].reset();
    }

    void reset() {
        assert(board != nullptr);

        idx = 0;
        accum_table->reset();
        accum_table->refresh(*board, WHITE);
        accum_table->refresh(*board, BLACK);
    }

    void refresh(Color view) {
        assert(board != nullptr);
        accum_table->refresh(*board, view);
    }

    Accum &operator[](int i) {
        assert(i >= 0 && i <= MAX_PLY);
        return data[i];
    }

    const Accum &operator[](int i) const {
        assert(i >= 0 && i <= MAX_PLY);
        return data[i];
    }

    Accum &back() {
        assert(idx >= 0 && idx <= MAX_PLY);
        return data[idx];
    }

    const Accum &back() const {
        assert(idx >= 0 && idx <= MAX_PLY);
        return data[idx];
    }

    int get_idx() const {
        return idx;
    }

  private:
    int idx;
    Board *board;

    std::array<Accum, MAX_PLY + 1> data;
    std::unique_ptr<AccumTable> accum_table = std::make_unique<AccumTable>(AccumTable());
};

} // namespace NNUE
