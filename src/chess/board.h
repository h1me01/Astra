#pragma once

#include <array>
#include <memory>

#include "../nnue/accum.h"
#include "castling_rights.h"
#include "cuckoo.h"
#include "state_info.h"
#include "types.h"
#include "zobrist.h"

namespace chess {

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

    void set_fen(const std::string &fen);
    std::string fen() const;

    void reset_ply();

    nnue::DirtyPieces make_move(Move move);
    void undo_move(Move move);

    void make_nullmove();
    void undo_nullmove();

    void perft(int depth);

    bool legal(Move move) const;
    bool pseudo_legal(Move move) const;

    bool in_check() const;
    bool is_draw(int ply) const;
    bool non_pawn_mat(Color c) const;
    bool gives_check(Move move) const;
    bool upcoming_repetition(int ply) const;
    bool see(Move move, int threshold) const;

    Threats threats() const;

    U64 attackers_to(Color c, Square sq, U64 occ) const;

    U64 occupancy(Color c) const;
    U64 occupancy() const;

    U64 piece_bb(Color c, PieceType pt) const;
    U64 piece_bb(PieceType pt) const;

    U64 diag_sliders(Color c) const;
    U64 orth_sliders(Color c) const;

    U64 check_squares(PieceType pt) const;

    U64 hash() const;
    U64 pawn_hash() const;
    U64 non_pawn_hash(Color c) const;

    Piece piece_at(Square sq) const;
    Piece captured_piece(Move move) const;

    Square king_sq(Color c) const;
    Square king_sq() const;

    Square en_passant() const;

    Color side_to_move() const;

    template <PieceType pt> int count(Color c) const;
    template <PieceType pt> int count() const;

    int ply_count() const;
    int fifty_move_count() const;

    StateInfo &state();
    const StateInfo &state() const;

  private:
    Color stm;
    Piece board[NUM_SQUARES];
    U64 pieces_bb[NUM_PIECES];
    U64 occ[NUM_COLORS];
    StateInfoList state_list;

    void update_hash(Piece pc, U64 hash);
    void put_piece(Piece pc, Square sq);
    void remove_piece(Square sq);
    void move_piece(Square from, Square to);

    void init_movegen_info();

    std::pair<Square, Square> castle_rook_sqs(Color c, Square to);
};

inline void Board::reset_ply() {
    state_list.reset();
    state().plies_from_null = 0;
}

inline bool Board::in_check() const {
    return state().checkers > 0;
}

// doesn't include stalemate
inline bool Board::is_draw(int ply) const {
    const StateInfo &info = state();
    return info.fmr_counter > 99 || (info.repetition && info.repetition < ply);
}

// checks if there is any non-pawn material on the board of the current side to move
inline bool Board::non_pawn_mat(Color c) const {
    for(auto pt : {KNIGHT, BISHOP, ROOK, QUEEN})
        if(piece_bb(c, pt))
            return true;
    return false;
}

inline U64 Board::attackers_to(Color c, Square sq, const U64 occ) const {
    return (pawn_attacks_bb(~c, sq) & piece_bb(c, PAWN)) |   //
           (attacks_bb<KNIGHT>(sq) & piece_bb(c, KNIGHT)) |  //
           (attacks_bb<BISHOP>(sq, occ) & diag_sliders(c)) | //
           (attacks_bb<ROOK>(sq, occ) & orth_sliders(c)) |   //
           (attacks_bb<KING>(sq) & piece_bb(c, KING));
}

inline U64 Board::occupancy(Color c) const {
    return occ[c];
}

inline U64 Board::occupancy() const {
    return occupancy(WHITE) | occupancy(BLACK);
}

inline U64 Board::piece_bb(Color c, PieceType pt) const {
    assert(valid_color(c) && valid_piece_type(pt));
    return pieces_bb[make_piece(c, pt)];
}

inline U64 Board::piece_bb(PieceType pt) const {
    assert(valid_piece_type(pt));
    return piece_bb(WHITE, pt) | piece_bb(BLACK, pt);
}

inline U64 Board::diag_sliders(Color c) const {
    return piece_bb(c, BISHOP) | piece_bb(c, QUEEN);
}

inline U64 Board::orth_sliders(Color c) const {
    return piece_bb(c, ROOK) | piece_bb(c, QUEEN);
}

inline U64 Board::check_squares(PieceType pt) const {
    assert(pt >= PAWN && pt <= KING);
    return state().check_squares[pt];
}

inline U64 Board::hash() const {
    return state().hash;
}

inline U64 Board::pawn_hash() const {
    return state().pawn_hash;
}

inline U64 Board::non_pawn_hash(Color c) const {
    assert(valid_color(c));
    return state().non_pawn_hash[c];
}

inline Piece Board::piece_at(Square sq) const {
    assert(valid_sq(sq));
    return board[sq];
}

inline Piece Board::captured_piece(Move move) const {
    assert(move);
    if(move.is_ep())
        return make_piece(~stm, PAWN);
    if(move.is_cap())
        return piece_at(move.to());
    return NO_PIECE;
}

inline Square Board::king_sq(Color c) const {
    return lsb(piece_bb(c, KING));
}

inline Square Board::king_sq() const {
    return king_sq(stm);
}

inline Square Board::en_passant() const {
    return state().ep_sq;
}

inline Color Board::side_to_move() const {
    return stm;
}

template <PieceType pt> //
inline int Board::count(Color c) const {
    int count = 0;
    if(c != BLACK)
        count += pop_count(piece_bb(WHITE, pt));
    if(c != WHITE)
        count += pop_count(piece_bb(BLACK, pt));
    return count;
}

template <PieceType pt> //
inline int Board::count() const {
    return pop_count(piece_bb(pt));
}

inline int Board::ply_count() const {
    assert(state_list.size() > 0);
    return state_list.size() - 1;
}

inline int Board::fifty_move_count() const {
    return state().fmr_counter;
}

inline StateInfo &Board::state() {
    return state_list.back();
}

inline const StateInfo &Board::state() const {
    return state_list.back();
}

// private functions

inline void Board::update_hash(Piece pc, U64 hash) {
    assert(valid_piece(pc));

    StateInfo &info = state();

    info.hash ^= hash;
    if(piece_type(pc) == PAWN)
        info.pawn_hash ^= hash;
    else
        info.non_pawn_hash[piece_color(pc)] ^= hash;
}

inline void Board::put_piece(Piece pc, Square sq) {
    assert(valid_piece(pc) && valid_sq(sq));

    board[sq] = pc;
    pieces_bb[pc] |= sq_bb(sq);
    occ[piece_color(pc)] |= sq_bb(sq);

    update_hash(pc, zobrist::psq_hash(pc, sq));
}

inline void Board::remove_piece(Square sq) {
    assert(valid_sq(sq));

    Piece pc = piece_at(sq);
    assert(valid_piece(pc));

    pieces_bb[pc] ^= sq_bb(sq);
    board[sq] = NO_PIECE;
    occ[piece_color(pc)] ^= sq_bb(sq);

    update_hash(pc, zobrist::psq_hash(pc, sq));
}

inline void Board::move_piece(Square from, Square to) {
    assert(valid_sq(from) && valid_sq(to));

    Piece pc = piece_at(from);
    assert(valid_piece(pc));

    pieces_bb[pc] ^= sq_bb(from) | sq_bb(to);
    board[to] = pc;
    board[from] = NO_PIECE;
    occ[piece_color(pc)] ^= sq_bb(from) | sq_bb(to);

    update_hash(pc, zobrist::psq_hash(pc, to) ^ zobrist::psq_hash(pc, from));
}

} // namespace chess
