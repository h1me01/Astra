#pragma once

#include <cassert>
#include <memory>
#include <string>

#include "../ndarray.h"
#include "../nnue/accumulator.h"
#include "bitboard.h"
#include "cuckoo.h"
#include "state.h"
#include "types.h"
#include "zobrist.h"

namespace astra {

class Board {
  public:
    Board() = default;
    Board(const std::string fen);

    void print() const;

    void set_fen(const std::string& fen);
    std::string fen() const;

    void reset_ply();

    nnue::DirtyPieceStack make_move(Move move);
    void undo_move(Move move);

    void make_move();
    void undo_move();

    bool is_legal(Move move) const;
    bool is_pseudo_legal(Move move) const;
    bool in_check() const;
    bool is_draw(int ply) const;
    bool non_pawn_mat(Color c) const;
    bool gives_check(Move move) const;
    bool upcoming_repetition(int ply) const;
    bool see(Move move, int threshold) const;

    Bitboard attackers_to(Color c, Square sq, Bitboard occ) const;
    template <PieceType pt>
    Bitboard attacks_by(Color c) const;
    Bitboard occupancy(Color c) const;
    Bitboard occupancy() const;
    template <PieceType pt>
    Bitboard piece_bb() const;
    template <PieceType pt>
    Bitboard piece_bb(Color c) const;
    Bitboard piece_bb(Color c, PieceType pt) const;
    Bitboard diag_sliders(Color c) const;
    Bitboard orth_sliders(Color c) const;
    Bitboard check_squares(PieceType pt) const;

    Hash hash() const;
    Hash pawn_hash() const;
    Hash minor_piece_hash() const;
    Hash non_pawn_hash(Color c) const;

    Piece piece_at(Square sq) const;
    Piece capture_piece(Move move) const;

    Square king_sq(Color c) const;
    Square king_sq() const;
    Square en_passant() const;

    Color side_to_move() const;

    template <PieceType pt>
    int count(Color c) const;
    template <PieceType pt>
    int count() const;

    int ply_count() const;
    int fifty_move_count() const;

    StateInfo& state();
    const StateInfo& state() const;

  private:
    Color stm_;
    NDArray<Bitboard, NUM_COLORS> occ_;
    NDArray<Bitboard, NUM_PIECES> piece_bb_;
    NDArray<Piece, NUM_SQUARES> board_;
    StateInfoStack states_;

    void update_hash(Piece pc, Hash hash);
    void put_piece(Piece pc, Square sq);
    void remove_piece(Square sq);
    void move_piece(Square from, Square to);

    void init_movegen_info();
};

inline void Board::reset_ply() {
    auto current_state = state();
    states_.clear();
    StateInfo& info = states_.push();
    info = current_state;
    info.plies_from_null = 0;
}

inline bool Board::in_check() const { return state().checkers > 0; }

// doesn't include stalemate
inline bool Board::is_draw(int ply) const {
    const StateInfo& info = state();
    return info.fifty_move_counter > 99 || (info.repetition && info.repetition < ply);
}

inline bool Board::non_pawn_mat(Color c) const {
    return piece_bb<KNIGHT>(c) || piece_bb<BISHOP>(c) || piece_bb<ROOK>(c) || piece_bb<QUEEN>(c);
}

inline Bitboard Board::attackers_to(Color c, Square sq, const Bitboard occ) const {
    return (pawn_attacks_bb(~c, sq) & piece_bb<PAWN>(c)) |   //
           (attacks_bb<KNIGHT>(sq) & piece_bb<KNIGHT>(c)) |  //
           (attacks_bb<BISHOP>(sq, occ) & diag_sliders(c)) | //
           (attacks_bb<ROOK>(sq, occ) & orth_sliders(c)) |   //
           (attacks_bb<KING>(sq) & piece_bb<KING>(c));
}

template <PieceType pt>
inline Bitboard Board::attacks_by(Color c) const {
    if constexpr (pt == PAWN) {
        return (c == WHITE) ? pawn_attacks_bb<WHITE>(piece_bb<PAWN>(WHITE)) //
                            : pawn_attacks_bb<BLACK>(piece_bb<PAWN>(BLACK));
    } else {
        Bitboard attacks = 0;
        Bitboard attackers = piece_bb<pt>(c);
        while (attackers)
            attacks |= attacks_bb<pt>(pop_lsb(attackers), occupancy());
        return attacks;
    }
}

inline Bitboard Board::occupancy(Color c) const {
    assert(is_valid(c));
    return occ_(c);
}

inline Bitboard Board::occupancy() const { return occupancy(WHITE) | occupancy(BLACK); }

template <PieceType pt>
inline Bitboard Board::piece_bb(Color c) const {
    assert(is_valid(c));
    assert(is_valid(pt));
    return piece_bb_(make_piece(c, pt));
}

inline Bitboard Board::piece_bb(Color c, PieceType pt) const {
    assert(is_valid(c));
    assert(is_valid(pt));
    return piece_bb_(make_piece(c, pt));
}

template <PieceType pt>
inline Bitboard Board::piece_bb() const {
    assert(is_valid(pt));
    return piece_bb<pt>(WHITE) | piece_bb<pt>(BLACK);
}

inline Bitboard Board::diag_sliders(Color c) const { return piece_bb<BISHOP>(c) | piece_bb<QUEEN>(c); }
inline Bitboard Board::orth_sliders(Color c) const { return piece_bb<ROOK>(c) | piece_bb<QUEEN>(c); }

inline Bitboard Board::check_squares(PieceType pt) const {
    assert(is_valid(pt));
    return state().check_squares(pt);
}

inline Hash Board::hash() const { return state().hash; }
inline Hash Board::pawn_hash() const { return state().pawn_hash; }
inline Hash Board::minor_piece_hash() const { return state().minor_piece_hash; }

inline Hash Board::non_pawn_hash(Color c) const {
    assert(is_valid(c));
    return state().non_pawn_hash(c);
}

inline Piece Board::piece_at(Square sq) const {
    assert(is_valid(sq));
    return board_(sq);
}

// returns the piece that would be captured by the given move
inline Piece Board::capture_piece(Move move) const {
    assert(move);
    if (move.is_ep())
        return make_piece(~stm_, PAWN);
    if (move.is_cap())
        return piece_at(move.to());
    return NO_PIECE;
}

inline Square Board::king_sq(Color c) const { return lsb(piece_bb<KING>(c)); }
inline Square Board::king_sq() const { return king_sq(stm_); }
inline Square Board::en_passant() const { return state().ep_sq; }
inline Color Board::side_to_move() const { return stm_; }

template <PieceType pt>
inline int Board::count(Color c) const {
    return pop_count(piece_bb<pt>(c));
}

template <PieceType pt>
inline int Board::count() const {
    return pop_count(piece_bb<pt>());
}

inline int Board::ply_count() const {
    assert(states_.size() > 0);
    return states_.size() - 1;
}

inline int Board::fifty_move_count() const { return state().fifty_move_counter; }
inline StateInfo& Board::state() { return states_.back(); }
inline const StateInfo& Board::state() const { return states_.back(); }

// private functions

inline void Board::update_hash(Piece pc, Hash hash) {
    assert(is_valid(pc));

    StateInfo& info = state();
    PieceType pt = type_of(pc);

    info.hash ^= hash;
    if (pt == PAWN) {
        info.pawn_hash ^= hash;
    } else {
        info.non_pawn_hash(color_of(pc)) ^= hash;
        if (pt <= BISHOP)
            info.minor_piece_hash ^= hash;
    }
}

inline void Board::put_piece(Piece pc, Square sq) {
    assert(is_valid(sq));
    assert(is_valid(pc));

    board_(sq) = pc;
    piece_bb_(pc) |= sq_bb(sq);
    occ_(color_of(pc)) |= sq_bb(sq);

    update_hash(pc, zobrist::psq(pc, sq));
}

inline void Board::remove_piece(Square sq) {
    assert(is_valid(sq));

    Piece pc = piece_at(sq);
    assert(is_valid(pc));

    piece_bb_(pc) ^= sq_bb(sq);
    board_(sq) = NO_PIECE;
    occ_(color_of(pc)) ^= sq_bb(sq);

    update_hash(pc, zobrist::psq(pc, sq));
}

inline void Board::move_piece(Square from, Square to) {
    assert(is_valid(to));
    assert(is_valid(from));

    Piece pc = piece_at(from);
    assert(is_valid(pc));

    piece_bb_(pc) ^= sq_bb(from) | sq_bb(to);
    board_(to) = pc;
    board_(from) = NO_PIECE;
    occ_(color_of(pc)) ^= sq_bb(from) | sq_bb(to);

    update_hash(pc, zobrist::psq(pc, to) ^ zobrist::psq(pc, from));
}

} // namespace astra
