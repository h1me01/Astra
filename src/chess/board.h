#pragma once

#include <array>
#include <memory>

#include "../nnue/accum.h"
#include "cuckoo.h"
#include "state_info.h"
#include "zobrist.h"

namespace Chess {

class Board {
  public:
    Board(const std::string &fen, bool update_nnue = true);

    Board &operator=(const Board &other);

    void print() const;

    void make_move(const Move &m, bool update_nnue = true);
    void undo_move(const Move &m);

    void make_nullmove();
    void undo_nullmove();

    void update_accums();

    void perft(int depth);

    int get_phase() const;

    bool is_legal(const Move &m) const;
    bool is_pseudolegal(const Move &m) const;

    bool is_repetition(int ply) const;
    bool is_draw(int ply) const;

    bool nonpawn_mat(Color c) const;

    bool see(Move &m, int threshold) const;
    bool has_upcoming_repetition(int ply);

    void set_fen(const std::string &fen, bool update_nnue = true);
    std::string get_fen() const;

    U64 get_occupancy(Color c) const;
    U64 attackers_to(Color c, Square sq, U64 occ) const;
    U64 key_after(Move m) const;

    void reset_accums();
    void reset_ply();

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

    bool in_check() const {
        return states[curr_ply].checkers > 0;
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

    U64 get_pawn_hash() const {
        return states[curr_ply].pawn_hash;
    }

    U64 get_nonpawn_hash(Color c) const {
        assert(valid_color(c));
        return states[curr_ply].non_pawn_hash[c];
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

    NNUE::Accum &get_accum() {
        return accum_list.back();
    }

    const NNUE::Accum &get_accum() const {
        return accum_list.back();
    }

  private:
    Color stm;
    int curr_ply;
    Piece board[NUM_SQUARES];
    U64 piece_bb[NUM_PIECES];

    NNUE::AccumList accum_list;
    std::array<StateInfo, 512> states;

    void put_piece(Piece pc, Square sq, bool update_nnue = false);
    void remove_piece(Square sq, bool update_nnue = false);
    void move_piece(Square from, Square to, bool update_nnue = false);

    void init_threats();
    void init_slider_blockers();
};

inline void Board::reset_accums() {
    accum_list.reset();
}

inline void Board::reset_ply() {
    states[0] = states[curr_ply];
    states[0].plies_from_null = 0;
    curr_ply = 0;
}

inline int Board::get_phase() const {
    int phase = 3 * pop_count(piece_bb[WHITE_KNIGHT] | piece_bb[BLACK_KNIGHT]);
    phase += 3 * pop_count(piece_bb[WHITE_BISHOP] | piece_bb[BLACK_BISHOP]);
    phase += 5 * pop_count(piece_bb[WHITE_ROOK] | piece_bb[BLACK_ROOK]);
    phase += 10 * pop_count(piece_bb[WHITE_QUEEN] | piece_bb[BLACK_QUEEN]);
    return phase;
}

inline U64 Board::get_occupancy(Color c = BOTH_COLORS) const {
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

inline U64 Board::key_after(Move m) const {
    U64 new_hash = states[curr_ply].hash;

    if(!m)
        return new_hash ^ Zobrist::get_side();

    Square from = m.from();
    Square to = m.to();

    Piece pc = piece_at(from);
    Piece captured = piece_at(to);

    if(captured != NO_PIECE)
        new_hash ^= Zobrist::get_psq(captured, to);

    new_hash ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);
    new_hash ^= Zobrist::get_side();

    return new_hash;
}

// doesn't include stalemate
inline bool Board::is_draw(int ply) const {
    return states[curr_ply].fmr_counter > 99 || is_repetition(ply);
}

// checks if there is any non-pawn material on the board of the current side to move
inline bool Board::nonpawn_mat(Color c) const {
    return get_piecebb(c, KNIGHT) | get_piecebb(c, BISHOP) | get_piecebb(c, ROOK) | get_piecebb(c, QUEEN);
}

inline void Board::put_piece(Piece pc, Square sq, bool update_nnue) {
    assert(valid_sq(sq));
    assert(valid_piece(pc));

    board[sq] = pc;
    piece_bb[pc] |= square_bb(sq);
    states[curr_ply].occ[piece_color(pc)] ^= square_bb(sq);

    if(!update_nnue)
        return;

    NNUE::Accum &acc = get_accum();
    acc.put_piece(pc, sq, king_sq(WHITE), king_sq(BLACK));
}

inline void Board::remove_piece(Square sq, bool update_nnue) {
    assert(valid_sq(sq));

    Piece pc = board[sq];
    assert(pc != NO_PIECE);

    piece_bb[pc] ^= square_bb(sq);
    board[sq] = NO_PIECE;
    states[curr_ply].occ[piece_color(pc)] ^= square_bb(sq);

    if(!update_nnue)
        return;

    NNUE::Accum &acc = get_accum();
    acc.remove_piece(pc, sq, king_sq(WHITE), king_sq(BLACK));
}

inline void Board::move_piece(Square from, Square to, bool update_nnue) {
    assert(valid_sq(to));
    assert(valid_sq(from));

    Piece pc = board[from];
    assert(valid_piece(pc));

    piece_bb[pc] ^= square_bb(from) | square_bb(to);
    board[to] = pc;
    board[from] = NO_PIECE;
    states[curr_ply].occ[piece_color(pc)] ^= square_bb(from) | square_bb(to);

    if(!update_nnue)
        return;

    NNUE::Accum &acc = get_accum();
    acc.move_piece(pc, from, to, king_sq(WHITE), king_sq(BLACK));

    if(NNUE::needs_refresh(pc, from, to))
        acc.set_refresh(piece_color(pc));
}

} // namespace Chess
