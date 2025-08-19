#pragma once

#include <array>

#include "state_info.h"
#include "zobrist.h"

namespace Chess {

class Board {
  public:
    Board(const std::string &fen);

    Board &operator=(const Board &other);

    void print() const;

    void make_move(const Move &m);
    void unmake_move(const Move &m);

    void perft(int depth);

    bool in_check() const {
        return states[curr_ply].checkers > 0;
    }

    bool is_legal(const Move &m) const;

    bool is_pseudolegal(const Move &m) const;

    bool is_repetition(int ply) const;
    bool is_draw(int ply) const;

    void reset_ply() {
        states[0] = states[curr_ply];
        curr_ply = 0;
    }

    void set_fen(const std::string &fen);

    std::string get_fen() const;

    U64 occupancy(Color c) const;

    U64 attackers_to(Color c, Square sq, U64 occ) const;

    U64 get_piecebb(Color c, PieceType pt) const {
        assert(valid_color(c));
        assert(valid_piece_type(pt));
        return piece_bb[make_piece(c, pt)];
    }

    Piece piece_at(Square sq) const {
        assert(valid_sq(sq));
        return board[sq];
    }

    U64 get_threats(PieceType pt) const {
        assert(valid_piece_type(pt));
        return states[curr_ply].threats[pt];
    }

    U64 diag_sliders(Color c) const {
        return get_piecebb(c, BISHOP) | get_piecebb(c, QUEEN);
    }

    U64 orth_sliders(Color c) const {
        return get_piecebb(c, ROOK) | get_piecebb(c, QUEEN);
    }

    Color get_stm() const {
        return stm;
    }

    int get_ply() const {
        return curr_ply;
    }

    int get_fmr() const {
        return states[curr_ply].fmr_counter;
    }

    U64 get_hash() const {
        return states[curr_ply].hash;
    }

    Square king_sq(Color c) const {
        return lsb(get_piecebb(c, KING));
    }

    StateInfo &get_state() {
        return states[curr_ply];
    }

    const StateInfo &get_state() const {
        return states[curr_ply];
    }

  private:
    std::array<StateInfo, 512> states;

    Color stm;
    int curr_ply;
    Piece board[NUM_SQUARES];
    U64 piece_bb[NUM_PIECES];

    void put_piece(Piece pc, Square sq);
    void remove_piece(Square sq);
    void move_piece(Square from, Square to);

    void init_threats();
    void init_slider_blockers();
};

inline U64 Board::occupancy(Color c = BOTH_COLORS) const {
    U64 occ = 0;
    if(c != BLACK)
        occ |= states[curr_ply].occ[WHITE];
    if(c != WHITE)
        occ |= states[curr_ply].occ[BLACK];
    return occ;
}

inline U64 Board::attackers_to(Color c, Square sq, const U64 occ) const {
    U64 attacks = get_pawn_attacks(~c, sq) & get_piecebb(c, PAWN);
    attacks |= get_attacks(KNIGHT, sq) & get_piecebb(c, KNIGHT);
    attacks |= get_attacks(BISHOP, sq, occ) & diag_sliders(c);
    attacks |= get_attacks(ROOK, sq, occ) & orth_sliders(c);
    attacks |= get_attacks(KING, sq) & get_piecebb(c, KING);
    return attacks;
}

inline bool Board::is_repetition(int ply) const {
    for(int i = 0; i < ply; i++) {
        if(states[i].hash == states[ply].hash)
            return true;
    }
    return false;
}

// doesn't include stalemate
inline bool Board::is_draw(int ply) const {
    int num_pieces = pop_count(occupancy());
    int num_knights = pop_count(get_piecebb(WHITE, KNIGHT) | get_piecebb(BLACK, KNIGHT));
    int num_bishops = pop_count(get_piecebb(WHITE, BISHOP) | get_piecebb(BLACK, BISHOP));

    if(num_pieces == 2)
        return true;
    if(num_pieces == 3 && (num_knights == 1 || num_bishops == 1))
        return true;

    if(num_pieces == 4) {
        if(num_knights == 2)
            return true;
        if(num_bishops == 2 && pop_count(get_piecebb(WHITE, BISHOP)) == 1)
            return true;
    }

    return states[curr_ply].fmr_counter > 99 || is_repetition(ply);
}

inline void Board::put_piece(Piece pc, Square sq) {
    assert(valid_sq(sq));
    assert(valid_piece(pc));

    board[sq] = pc;
    piece_bb[pc] |= square_bb(sq);
    states[curr_ply].occ[piece_color(pc)] ^= square_bb(sq);
}

inline void Board::remove_piece(Square sq) {
    assert(valid_sq(sq));

    Piece pc = board[sq];
    assert(pc != NO_PIECE);

    piece_bb[pc] ^= square_bb(sq);
    board[sq] = NO_PIECE;
    states[curr_ply].occ[piece_color(pc)] ^= square_bb(sq);
}

inline void Board::move_piece(Square from, Square to) {
    assert(valid_sq(to));
    assert(valid_sq(from));

    Piece pc = board[from];
    assert(valid_piece(pc));

    piece_bb[pc] ^= square_bb(from) | square_bb(to);
    board[to] = pc;
    board[from] = NO_PIECE;
    states[curr_ply].occ[piece_color(pc)] ^= square_bb(from) | square_bb(to);
}

} // namespace Chess
