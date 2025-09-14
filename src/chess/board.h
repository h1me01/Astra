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

    int repetition;

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

    template <bool update_accum = true> //
    void make_move(const Move &move);
    void undo_move(const Move &move);

    void make_nullmove();
    void undo_nullmove();

    void update_accums();

    void perft(int depth);

    bool is_legal(const Move &move) const;
    bool is_pseudolegal(const Move &move) const;

    bool is_repetition(int ply) const;
    bool is_draw(int ply) const;

    bool nonpawn_mat(Color c) const;

    bool see(Move &move, int threshold) const;
    bool upcoming_repetition(int ply);

    void set_fen(const std::string &fen, bool init_accum = true);
    std::string get_fen() const;

    U64 get_occupancy(Color c) const;
    U64 attackers_to(Color c, Square sq, U64 occ) const;

    void reset_accum_list();
    void reset_ply();

    U64 get_piecebb(Color c, PieceType pt) const {
        assert(valid_color(c));
        assert(valid_piece_type(pt));
        return piece_bb[make_piece(c, pt)];
    }

    U64 get_piecebb(PieceType pt) const {
        assert(valid_piece_type(pt));
        return get_piecebb(WHITE, pt) | get_piecebb(BLACK, pt);
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

    template <bool update_accum = false> //
    void put_piece(Piece pc, Square sq);

    template <bool update_accum = false> //
    void remove_piece(Square sq);

    template <bool update_accum = false> //
    void move_piece(Square from, Square to);

    void init_threats();
    void init_slider_blockers();

    std::pair<Square, Square> get_castle_rook_squares(Color c, Square to);
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

// doesn't include stalemate
inline bool Board::is_draw(int ply) const {
    return get_state().fmr_counter > 99 || is_repetition(ply);
}

// checks if there is any non-pawn material on the board of the current side to move
inline bool Board::nonpawn_mat(Color c) const {
    return get_piecebb(c, KNIGHT) | get_piecebb(c, BISHOP) | get_piecebb(c, ROOK) | get_piecebb(c, QUEEN);
}

template <bool update_accum> //
void Board::make_move(const Move &move) {
    const Square from = move.from();
    const Square to = move.to();
    const Piece pc = piece_at(from);
    const PieceType pt = piece_type(pc);
    const Piece captured = move.is_ep() ? make_piece(~stm, PAWN) : piece_at(to);

    assert(move);
    assert(valid_piece(pc));
    assert(piece_type(captured) != KING);
    assert(valid_piece(captured) == move.is_cap());

    // update history
    curr_ply++;
    states[curr_ply] = StateInfo(states[curr_ply - 1]);
    StateInfo &info = get_state();

    // update move counters
    info.fmr_counter++;
    info.plies_from_null++;

    // reset half move clock if pawn move or capture
    if(pt == PAWN || move.is_cap())
        info.fmr_counter = 0;

    // reset ep square if it exists
    if(valid_sq(info.ep_sq)) {
        info.hash ^= Zobrist::get_ep(info.ep_sq); // remove ep square from hash
        info.ep_sq = NO_SQUARE;
    }

    // increment accum
    if(update_accum)
        accum_list.push();

    if(move.is_castling()) {
        assert(pt == KING);

        auto [rook_from, rook_to] = get_castle_rook_squares(stm, to);
        assert(piece_at(rook_from) == make_piece(stm, ROOK));

        move_piece<update_accum>(rook_from, rook_to);
    }

    if(move.is_cap()) {
        Square cap_sq = move.is_ep() ? Square(to ^ 8) : to;
        remove_piece<update_accum>(cap_sq);
    }

    // move current piece
    move_piece<update_accum>(from, to);

    if(pt == PAWN) {
        // set ep square if double push can be captured by enemy pawn
        auto ep_sq = Square(to ^ 8);
        if((from ^ to) == 16 && (get_pawn_attacks(stm, ep_sq) & get_piecebb(~stm, PAWN))) {
            info.ep_sq = ep_sq;
            info.hash ^= Zobrist::get_ep(ep_sq); // add ep square to hash
        } else if(move.is_prom()) {
            PieceType prom_t = move.prom_type();
            Piece prom_pc = make_piece(stm, prom_t);

            assert(prom_t != PAWN);
            assert(valid_piece(prom_pc));

            // add promoted piece and remove pawn
            remove_piece<update_accum>(to);
            put_piece<update_accum>(prom_pc, to);
        }
    }

    // update castling rights if king/rook moves or if one of the rooks get captured
    if(info.castle_rights.on_castle_sq(from) || info.castle_rights.on_castle_sq(to)) {
        // remove old castling rights from hash
        info.hash ^= Zobrist::get_castle(info.castle_rights.get_hash_idx());
        info.castle_rights.update(square_bb(from), square_bb(to));
        // add new castling rights to hash
        info.hash ^= Zobrist::get_castle(info.castle_rights.get_hash_idx());
    }

    // update hash
    info.hash ^= Zobrist::get_side();

    stm = ~stm;
    info.captured = captured;

    init_threats();
    init_slider_blockers();

    // update repetition info for upcoming repetition detection

    info.repetition = 0;
    int distance = std::min(info.fmr_counter, info.plies_from_null);

    if(distance >= 4) {
        StateInfo *prev = &states[curr_ply - 2];
        for(int i = 4; i <= distance; i += 2) {
            prev -= 2;
            if(prev->hash == info.hash) {
                info.repetition = prev->repetition ? -i : i;
                break;
            }
        }
    }
}

inline void Board::undo_move(const Move &move) {
    const MoveType mt = move.type();
    const Square from = move.from();
    const Square to = move.to();
    const Piece captured = get_state().captured;

    assert(move);
    assert(valid_piece(piece_at(to)));
    assert(!valid_piece(piece_at(from)));

    stm = ~stm;

    // decrement accum
    accum_list.pop();

    if(move.is_prom()) {
        remove_piece(to);
        put_piece(make_piece(stm, PAWN), to);
    }

    if(mt == CASTLING) {
        auto [rook_to, rook_from] = get_castle_rook_squares(stm, to);

        move_piece(to, from); // move king
        move_piece(rook_from, rook_to);
    } else {
        move_piece(to, from);

        if(valid_piece(captured)) {
            Square cap_sqr = move.is_ep() ? Square(to ^ 8) : to;
            put_piece(captured, cap_sqr);
        }
    }

    curr_ply--;
}

inline void Board::make_nullmove() {
    curr_ply++;
    states[curr_ply] = StateInfo(states[curr_ply - 1]);
    StateInfo &info = get_state();

    info.fmr_counter++;
    info.plies_from_null = 0;

    if(valid_sq(info.ep_sq)) {
        info.hash ^= Zobrist::get_ep(info.ep_sq); // remove ep square from hash
        info.ep_sq = NO_SQUARE;
    }

    info.hash ^= Zobrist::get_side();

    stm = ~stm;

    init_threats();
    init_slider_blockers();
}

inline void Board::undo_nullmove() {
    curr_ply--;
    stm = ~stm;
}

// private member

template <bool update_accum> //
void Board::put_piece(Piece pc, Square sq) {
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

template <bool update_accum> //
void Board::remove_piece(Square sq) {
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

template <bool update_accum> //
void Board::move_piece(Square from, Square to) {
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

inline std::pair<Square, Square> Board::get_castle_rook_squares(Color c, Square to) {
    assert(valid_color(c));
    assert(to == rel_sq(c, g1) || to == rel_sq(c, c1));

    Square rook_from, rook_to;

    if(to == rel_sq(c, g1)) {
        // kingside
        rook_from = rel_sq(c, h1);
        rook_to = rel_sq(c, f1);
    } else {
        // queenside
        rook_from = rel_sq(c, a1);
        rook_to = rel_sq(c, d1);
    }

    return {rook_from, rook_to};
}

} // namespace Chess
