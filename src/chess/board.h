#pragma once

#include <memory>

#include "../net/accumulator.h"
#include "bitboard.h"
#include "cuckoo.h"
#include "misc.h"
#include "zobrist.h"

namespace Chess {

class CastlingRights {
  public:
    CastlingRights() : mask(0) {}

    void add_kingside(Color c) {
        mask |= OO_MASK[c];
    }

    void add_queenside(Color c) {
        mask |= OOO_MASK[c];
    }

    void update(U64 from, U64 to) {
        mask &= ~(from | to);
    }

    bool kingside(const Color c) const {
        assert(valid_color(c));
        return (mask & OO_MASK[c]) == OO_MASK[c];
    }

    bool queenside(const Color c) const {
        assert(valid_color(c));
        return (mask & OOO_MASK[c]) == OOO_MASK[c];
    }

    bool any(const Color c) const {
        return kingside(c) || queenside(c);
    }

    bool any() const {
        return any(WHITE) || any(BLACK);
    }

    bool on_castle_sq(Square sq) const {
        assert(valid_sq(sq));
        return mask & square_bb(sq);
    }

    int get_hash_idx() const {
        return kingside(WHITE) + 2 * queenside(WHITE) + 4 * kingside(BLACK) + 8 * queenside(BLACK);
    }

  private:
    static constexpr U64 OO_MASK[NUM_COLORS] = {0x90ULL, 0x9000000000000000ULL};
    static constexpr U64 OOO_MASK[NUM_COLORS] = {0x11ULL, 0x1100000000000000ULL};

    U64 mask;
};

struct StateInfo {
    StateInfo() = default;
    StateInfo(const StateInfo &other) = default;
    StateInfo &operator=(const StateInfo &other) = default;

    U64 hash;
    U64 pawn_hash;
    U64 non_pawn_hash[NUM_COLORS];
    Piece captured;
    Square ep_sq;
    CastlingRights castle_rights;
    U64 occ[NUM_COLORS] = {};
    U64 threats[NUM_PIECE_TYPES] = {};
    int half_move_clock;
    int plies_from_null;

    U64 checkers = 0;
    U64 danger = 0;

    U64 pinners[NUM_COLORS] = {};
    U64 blockers[NUM_COLORS] = {};
};

class Board {
  public:
    std::array<StateInfo, 512> history;

    Board(const std::string &fen, bool update_nnue = true);

    Board &operator=(const Board &other);

    void print() const;
    void set_fen(const std::string &fen, bool update_nnue = true);

    std::string get_fen() const;

    U64 get_piecebb(Color c, PieceType pt) const;
    Piece piece_at(Square sq) const;

    U64 get_nonpawnhash(Color c) const;
    U64 get_threats(PieceType pt) const;

    U64 diag_sliders(Color c) const;
    U64 orth_sliders(Color c) const;

    U64 occupancy(Color c) const;
    U64 attackers_to(Color c, Square sq, U64 occ) const;
    U64 key_after(Move m) const;

    void reset_accum();
    void update_accums();

    void make_move(const Move &m, bool update_nnue = true);
    void unmake_move(const Move &m);

    void make_nullmove();
    void unmake_nullmove();

    bool in_check() const;
    bool nonpawnmat(Color c) const;

    bool is_legal(const Move &m) const;
    bool is_pseudolegal(const Move &m) const;

    bool is_repetition(int ply) const;
    bool is_draw(int ply) const;

    bool see(Move &m, int threshold) const;
    bool has_upcoming_repetition(int ply);

    int get_phase() const;

    void reset_ply() {
        history[0] = history[curr_ply];
        history[0].plies_from_null = 0;
        curr_ply = 0;
    }

    Color get_stm() const {
        return stm;
    }

    int get_ply() const {
        return curr_ply;
    }

    int halfmoveclock() const {
        return history[curr_ply].half_move_clock;
    }

    U64 get_hash() const {
        return history[curr_ply].hash;
    }

    U64 get_pawnhash() const {
        return history[curr_ply].pawn_hash;
    }

    Square king_sq(Color c) const {
        return lsb(get_piecebb(c, KING));
    }

    NNUE::Accum &get_accum() {
        return accums[accums_idx];
    }

  private:
    Color stm;
    int curr_ply;
    Piece board[NUM_SQUARES];
    U64 piece_bb[NUM_PIECES];

    int accums_idx;
    std::array<NNUE::Accum, MAX_PLY + 1> accums;
    std::unique_ptr<NNUE::AccumTable> accum_table = std::make_unique<NNUE::AccumTable>(NNUE::AccumTable());

    void put_piece(Piece pc, Square sq, bool update_nnue);
    void remove_piece(Square sq, bool update_nnue);
    void move_piece(Square from, Square to, bool update_nnue);

    void init_threats();
    void init_slider_blockers();
};

inline U64 Board::get_piecebb(Color c, PieceType pt) const {
    assert(valid_color(c));
    assert(valid_piece_type(pt));
    return piece_bb[make_piece(c, pt)];
}

inline Piece Board::piece_at(Square sq) const {
    assert(valid_sq(sq));
    return board[sq];
}

inline U64 Board::get_nonpawnhash(Color c) const {
    assert(valid_color(c));
    return history[curr_ply].non_pawn_hash[c];
}

inline U64 Board::get_threats(PieceType pt) const {
    assert(valid_piece_type(pt));
    return history[curr_ply].threats[pt];
}

inline void Board::reset_accum() {
    accums_idx = 0;
    accum_table->reset();
    accum_table->refresh(*this, WHITE);
    accum_table->refresh(*this, BLACK);
}

inline U64 Board::diag_sliders(Color c) const {
    return get_piecebb(c, BISHOP) | get_piecebb(c, QUEEN);
}

inline U64 Board::orth_sliders(Color c) const {
    return get_piecebb(c, ROOK) | get_piecebb(c, QUEEN);
}

inline U64 Board::occupancy(Color c = BOTH_COLORS) const {
    U64 occ = 0;
    if(c != BLACK)
        occ |= history[curr_ply].occ[WHITE];
    if(c != WHITE)
        occ |= history[curr_ply].occ[BLACK];
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

inline int Board::get_phase() const {
    int phase = 3 * pop_count(piece_bb[WHITE_KNIGHT] | piece_bb[BLACK_KNIGHT]);
    phase += 3 * pop_count(piece_bb[WHITE_BISHOP] | piece_bb[BLACK_BISHOP]);
    phase += 5 * pop_count(piece_bb[WHITE_ROOK] | piece_bb[BLACK_ROOK]);
    phase += 10 * pop_count(piece_bb[WHITE_QUEEN] | piece_bb[BLACK_QUEEN]);
    return phase;
}

inline bool Board::in_check() const {
    return history[curr_ply].checkers > 0;
}

// checks if there is any non-pawn material on the board of the current side to move
inline bool Board::nonpawnmat(Color c) const {
    return get_piecebb(c, KNIGHT) | get_piecebb(c, BISHOP) | get_piecebb(c, ROOK) | get_piecebb(c, QUEEN);
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

    return history[curr_ply].half_move_clock > 99 || is_repetition(ply);
}

inline void Board::put_piece(Piece pc, Square sq, bool update_nnue) {
    assert(valid_sq(sq));
    assert(valid_piece(pc));

    board[sq] = pc;
    piece_bb[pc] |= square_bb(sq);
    history[curr_ply].occ[piece_color(pc)] ^= square_bb(sq);

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
    history[curr_ply].occ[piece_color(pc)] ^= square_bb(sq);

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
    history[curr_ply].occ[piece_color(pc)] ^= square_bb(from) | square_bb(to);

    if(!update_nnue)
        return;

    NNUE::Accum &acc = get_accum();
    acc.move_piece(pc, from, to, king_sq(WHITE), king_sq(BLACK));

    if(NNUE::needs_refresh(pc, from, to))
        acc.set_refresh(piece_color(pc));
}

} // namespace Chess
