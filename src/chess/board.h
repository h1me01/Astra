#pragma once

#include <array>
#include <memory>

#include "../nnue/accum.h"
#include "castling_rights.h"
#include "cuckoo.h"
#include "types.h"
#include "zobrist.h"

namespace Chess {

struct StateInfo {
    StateInfo() = default;
    StateInfo(const StateInfo &other) = default;
    StateInfo &operator=(const StateInfo &other) = default;

    Piece captured;
    Square ep_sq;
    CastlingRights castle_rights;

    int fmr_counter; // fifty move rule
    int plies_from_null;

    U64 hash;
    U64 pawn_hash;
    U64 non_pawn_hash[NUM_COLORS];

    U64 occ[NUM_COLORS];
    U64 threats[NUM_PIECE_TYPES];

    U64 checkers;
    U64 danger;

    U64 pinners[NUM_COLORS];
    U64 blockers[NUM_COLORS];
};

class Board {
  public:
    Board(const std::string &fen = STARTING_FEN, bool init_accum = true);

    Board &operator=(const Board &other);

    void print() const;

    void make_move(const Move &m, bool update_accum = true);
    void undo_move(const Move &m);

    void make_nullmove();
    void undo_nullmove();

    void update_accums();

    void perft(int depth);

    bool is_legal(const Move &m) const;
    bool is_pseudolegal(const Move &m) const;

    bool is_repetition(int ply) const;
    bool is_draw(int ply) const;

    bool nonpawn_mat(Color c) const;

    bool see(Move &m, int threshold) const;
    bool has_upcoming_repetition(int ply);

    void set_fen(const std::string &fen, bool init_accum = true);
    std::string get_fen() const;

    U64 get_occupancy(Color c) const;
    U64 attackers_to(Color c, Square sq, U64 occ) const;
    U64 key_after(Move m) const;

    void reset_accum_list();
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
        return get_state().threats[pt];
    }

    U64 diag_sliders(Color c) const {
        return get_piecebb(c, BISHOP) | get_piecebb(c, QUEEN);
    }

    U64 orth_sliders(Color c) const {
        return get_piecebb(c, ROOK) | get_piecebb(c, QUEEN);
    }

    bool in_check() const {
        return get_state().checkers > 0;
    }

    Color get_stm() const {
        return stm;
    }

    int get_ply() const {
        return curr_ply;
    }

    int get_fmr() const {
        return get_state().fmr_counter;
    }

    U64 get_hash() const {
        return get_state().hash;
    }

    U64 get_pawn_hash() const {
        return get_state().pawn_hash;
    }

    U64 get_nonpawn_hash(Color c) const {
        assert(valid_color(c));
        return get_state().non_pawn_hash[c];
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

    template <PieceType pt> //
    int count(Color c = BOTH_COLORS) const {
        int count = 0;
        if(c != BLACK)
            count += pop_count(get_piecebb(WHITE, pt));
        if(c != WHITE)
            count += pop_count(get_piecebb(BLACK, pt));
        return count;
    }

  private:
    // private variables

    Color stm;
    int curr_ply;
    Piece board[NUM_SQUARES];
    U64 piece_bb[NUM_PIECES];

    NNUE::AccumList accum_list;
    std::array<StateInfo, 512> states;

    // private functions

    void put_piece(Piece pc, Square sq, bool update_accum = false);
    void remove_piece(Square sq, bool update_accum = false);
    void move_piece(Square from, Square to, bool update_accum = false);

    void init_threats();
    void init_slider_blockers();
};

inline void Board::reset_accum_list() {
    accum_list.reset();
}

inline void Board::reset_ply() {
    states[0] = get_state();
    states[0].plies_from_null = 0;
    curr_ply = 0;
}

inline U64 Board::get_occupancy(Color c = BOTH_COLORS) const {
    U64 occ = 0;
    if(c != BLACK)
        occ |= get_state().occ[WHITE];
    if(c != WHITE)
        occ |= get_state().occ[BLACK];
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
    U64 new_hash = get_state().hash;

    if(!m)
        return new_hash ^ Zobrist::get_side();

    Square from = m.from();
    Square to = m.to();

    Piece pc = piece_at(from);
    Piece captured = piece_at(to);

    if(valid_piece(captured))
        new_hash ^= Zobrist::get_psq(captured, to);

    new_hash ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);
    new_hash ^= Zobrist::get_side();

    return new_hash;
}

// doesn't include stalemate
inline bool Board::is_draw(int ply) const {
    return get_state().fmr_counter > 99 || is_repetition(ply);
}

// checks if there is any non-pawn material on the board of the current side to move
inline bool Board::nonpawn_mat(Color c) const {
    return get_piecebb(c, KNIGHT) | get_piecebb(c, BISHOP) | get_piecebb(c, ROOK) | get_piecebb(c, QUEEN);
}

inline void Board::put_piece(Piece pc, Square sq, bool update_accum) {
    assert(valid_sq(sq));
    assert(valid_piece(pc));

    StateInfo &info = get_state();

    board[sq] = pc;
    piece_bb[pc] |= square_bb(sq);
    info.occ[piece_color(pc)] ^= square_bb(sq);

    // update hash
    info.hash ^= Zobrist::get_psq(pc, sq);
    if(piece_type(pc) == PAWN)
        info.pawn_hash ^= Zobrist::get_psq(pc, sq);
    else
        info.non_pawn_hash[stm] ^= Zobrist::get_psq(pc, sq);

    if(!update_accum)
        return;

    NNUE::Accum &acc = get_accum();
    acc.put_piece(pc, sq, king_sq(WHITE), king_sq(BLACK));
}

inline void Board::remove_piece(Square sq, bool update_accum) {
    assert(valid_sq(sq));

    StateInfo &info = get_state();

    Piece pc = board[sq];
    assert(valid_piece(pc));

    piece_bb[pc] ^= square_bb(sq);
    board[sq] = NO_PIECE;
    info.occ[piece_color(pc)] ^= square_bb(sq);

    // update hash
    info.hash ^= Zobrist::get_psq(pc, sq);
    if(piece_type(pc) == PAWN)
        info.pawn_hash ^= Zobrist::get_psq(pc, sq);
    else
        info.non_pawn_hash[~stm] ^= Zobrist::get_psq(pc, sq);

    if(!update_accum)
        return;

    NNUE::Accum &acc = get_accum();
    acc.remove_piece(pc, sq, king_sq(WHITE), king_sq(BLACK));
}

inline void Board::move_piece(Square from, Square to, bool update_accum) {
    assert(valid_sq(to));
    assert(valid_sq(from));

    StateInfo &info = get_state();

    Piece pc = board[from];
    assert(valid_piece(pc));

    piece_bb[pc] ^= square_bb(from) | square_bb(to);
    board[to] = pc;
    board[from] = NO_PIECE;
    info.occ[piece_color(pc)] ^= square_bb(from) | square_bb(to);

    // update hash
    info.hash ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);
    if(piece_type(pc) == PAWN)
        info.pawn_hash ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);
    else
        info.non_pawn_hash[stm] ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);

    if(!update_accum)
        return;

    NNUE::Accum &acc = get_accum();
    acc.move_piece(pc, from, to, king_sq(WHITE), king_sq(BLACK));

    if(NNUE::needs_refresh(pc, from, to))
        acc.set_refresh(piece_color(pc));
}

} // namespace Chess
