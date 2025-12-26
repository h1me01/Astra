#pragma once

#include <array>
#include <memory>

#include "../nnue/accum.h"
#include "castling_rights.h"
#include "cuckoo.h"
#include "types.h"
#include "zobrist.h"

namespace chess {

struct StateInfo {
    StateInfo() = default;
    StateInfo(const StateInfo &other) = default;
    StateInfo &operator=(const StateInfo &other) = default;

    Piece captured;
    Square ep_sq;
    CastlingRights castle_rights;

    int fmr_counter; // fifty move rule
    int plies_from_null;

    int repetition;

    U64 hash;
    U64 pawn_hash;
    U64 non_pawn_hash[NUM_COLORS];

    U64 occ[NUM_COLORS];

    U64 checkers;
    U64 pinners[NUM_COLORS];
    U64 blockers[NUM_COLORS];
    U64 check_squares[NUM_PIECE_TYPES];
};

class Threats {
  public:
    U64 operator[](PieceType pt) const {
        assert(pt >= PAWN && pt <= ROOK);
        return data[pt];
    }

    U64 &operator[](PieceType pt) {
        assert(pt >= PAWN && pt <= ROOK);
        return data[pt];
    }

  private:
    U64 data[4]{};
};

class Board {
  public:
    Board() = default;
    Board(const std::string fen);

    Board &operator=(const Board &other);

    void print() const;

    void reset_ply();

    void set_fen(const std::string &fen);
    std::string get_fen() const;

    nnue::DirtyPieces make_move(const Move &move);
    void undo_move(const Move &move);

    void make_nullmove();
    void undo_nullmove();

    void perft(int depth);

    bool is_legal(const Move &move) const;
    bool is_pseudo_legal(const Move &move) const;

    bool in_check() const;
    bool is_draw(int ply) const;
    bool nonpawn_mat(Color c) const;
    bool gives_check(const Move &move) const;
    bool upcoming_repetition(int ply) const;
    bool see(const Move &move, int threshold) const;

    U64 attackers_to(Color c, Square sq, U64 occ) const;

    template <PieceType pt> //
    int count(Color c = BOTH_COLORS) const;

    Threats get_threats() const;

    U64 get_occupancy(Color c = BOTH_COLORS) const {
        U64 occ = 0;
        if(c != BLACK)
            occ |= get_state().occ[WHITE];
        if(c != WHITE)
            occ |= get_state().occ[BLACK];
        return occ;
    }

    U64 get_piece_bb(Color c, PieceType pt) const {
        assert(valid_color(c));
        assert(valid_piece_type(pt));
        return piece_bb[make_piece(c, pt)];
    }

    template <PieceType pt> //
    U64 get_piece_bb(Color c) const {
        return get_piece_bb(c, pt);
    }

    U64 get_piece_bb(PieceType pt) const {
        assert(valid_piece_type(pt));
        return get_piece_bb(WHITE, pt) | get_piece_bb(BLACK, pt);
    }

    template <PieceType pt> //
    U64 get_piece_bb() const {
        return get_piece_bb(pt);
    }

    Piece piece_at(Square sq) const {
        assert(valid_sq(sq));
        return board[sq];
    }

    Piece captured_piece(const Move &move) const {
        assert(move);
        if(move.is_ep())
            return make_piece(~stm, PAWN);
        if(move.is_cap())
            return piece_at(move.to());
        return NO_PIECE;
    }

    U64 get_diag_sliders(Color c) const {
        return get_piece_bb<BISHOP>(c) | get_piece_bb<QUEEN>(c);
    }

    U64 get_orth_sliders(Color c) const {
        return get_piece_bb<ROOK>(c) | get_piece_bb<QUEEN>(c);
    }

    Square get_king_sq(Color c) const {
        return lsb(get_piece_bb<KING>(c));
    }

    Color get_stm() const {
        return stm;
    }

    int get_ply() const {
        return curr_ply;
    }

    int get_fmr_count() const {
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

    U64 get_check_squares(PieceType pt) const {
        assert(pt >= PAWN && pt <= KING);
        return get_state().check_squares[pt];
    }

    StateInfo &get_state() {
        return states[curr_ply];
    }

    const StateInfo &get_state() const {
        return states[curr_ply];
    }

  private:
    Color stm;
    int curr_ply;
    Piece board[NUM_SQUARES];
    U64 piece_bb[NUM_PIECES];

    std::array<StateInfo, 512> states;

    void update_hash(Piece pc, U64 hash);
    void put_piece(Piece pc, Square sq);
    void remove_piece(Square sq);
    void move_piece(Square from, Square to);

    void init_movegen_info();

    std::pair<Square, Square> get_castle_rook_sqs(Color c, Square to);
};

inline void Board::reset_ply() {
    states[0] = get_state();
    states[0].plies_from_null = 0;
    curr_ply = 0;
}

inline bool Board::in_check() const {
    return get_state().checkers > 0;
}

// doesn't include stalemate
inline bool Board::is_draw(int ply) const {
    const StateInfo &info = get_state();
    const bool is_repetition = info.repetition && info.repetition < ply;
    return get_state().fmr_counter > 99 || is_repetition;
}

// checks if there is any non-pawn material on the board of the current side to move
inline bool Board::nonpawn_mat(Color c) const {
    return get_piece_bb<KNIGHT>(c) | get_piece_bb<BISHOP>(c) | //
           get_piece_bb<ROOK>(c) | get_piece_bb<QUEEN>(c);
}

inline U64 Board::attackers_to(Color c, Square sq, const U64 occ) const {
    U64 attacks = get_pawn_attacks(~c, sq) & get_piece_bb<PAWN>(c);
    attacks |= get_attacks<KNIGHT>(sq) & get_piece_bb<KNIGHT>(c);
    attacks |= get_attacks<BISHOP>(sq, occ) & get_diag_sliders(c);
    attacks |= get_attacks<ROOK>(sq, occ) & get_orth_sliders(c);
    attacks |= get_attacks<KING>(sq) & get_piece_bb<KING>(c);
    return attacks;
}

template <PieceType pt> //
inline int Board::count(Color c) const {
    int count = 0;
    if(c != BLACK)
        count += pop_count(get_piece_bb(WHITE, pt));
    if(c != WHITE)
        count += pop_count(get_piece_bb(BLACK, pt));
    return count;
}

// private functions

inline void Board::update_hash(Piece pc, U64 hash) {
    assert(valid_piece(pc));

    StateInfo &info = get_state();

    info.hash ^= hash;
    if(piece_type(pc) == PAWN)
        info.pawn_hash ^= hash;
    else
        info.non_pawn_hash[piece_color(pc)] ^= hash;
}

inline void Board::put_piece(Piece pc, Square sq) {
    assert(valid_sq(sq));
    assert(valid_piece(pc));

    StateInfo &info = get_state();

    board[sq] = pc;
    piece_bb[pc] |= sq_bb(sq);
    info.occ[piece_color(pc)] ^= sq_bb(sq);

    update_hash(pc, zobrist::get_psq(pc, sq));
}

inline void Board::remove_piece(Square sq) {
    assert(valid_sq(sq));

    StateInfo &info = get_state();

    Piece pc = piece_at(sq);
    assert(valid_piece(pc));

    piece_bb[pc] ^= sq_bb(sq);
    board[sq] = NO_PIECE;
    info.occ[piece_color(pc)] ^= sq_bb(sq);

    update_hash(pc, zobrist::get_psq(pc, sq));
}

inline void Board::move_piece(Square from, Square to) {
    assert(valid_sq(to));
    assert(valid_sq(from));

    StateInfo &info = get_state();

    Piece pc = piece_at(from);
    assert(valid_piece(pc));

    piece_bb[pc] ^= sq_bb(from) | sq_bb(to);
    board[to] = pc;
    board[from] = NO_PIECE;
    info.occ[piece_color(pc)] ^= sq_bb(from) | sq_bb(to);

    update_hash(pc, zobrist::get_psq(pc, to) ^ zobrist::get_psq(pc, from));
}

} // namespace chess
