#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <sstream>
#include <utility>

#include "../search/tune_params.h"
#include "../util.h"
#include "board.h"
#include "movegen.h"

namespace astra {

namespace {

std::pair<Square, Square> castling_rook_sqs(Color c, Square to) {
    assert(to == rel_sq(c, SQ_G1) || to == rel_sq(c, SQ_C1));
    bool ks = to == rel_sq(c, SQ_G1);
    return {rel_sq(c, ks ? SQ_H1 : SQ_A1), rel_sq(c, ks ? SQ_F1 : SQ_D1)};
}

inline Square sq_from(std::string_view sq_str) {
    assert(sq_str.size() >= 2);
    int file = sq_str[0] - 'a';
    int rank = sq_str[1] - '1';
    assert(file >= 0);
    assert(file < 8);
    assert(rank >= 0);
    assert(rank < 8);
    return static_cast<Square>(rank * 8 + file);
}

int piece_values_see(PieceType pt) {
    assert(valid_piece_type(pt) || pt == NO_PIECE_TYPE);

    switch (pt) {
    case PAWN:
        return search::pawn_value_see;
    case KNIGHT:
        return search::knight_value_see;
    case BISHOP:
        return search::bishop_value_see;
    case ROOK:
        return search::rook_value_see;
    case QUEEN:
        return search::queen_value_see;
    default:
        return 0;
    }
}

} // namespace

// Board

Board::Board(const std::string fen) { set_fen(fen); }

void Board::set_fen(const std::string& fen) {
    occ_.fill(0);
    piece_bb_.fill(0);
    board_.fill(NO_PIECE);

    states_.clear();
    StateInfo& info = state();

    std::vector<std::string> fen_parts = split(fen, ' ');
    if (fen_parts.size() != 6) {
        println("Invalid fen string: expected 6 parts but got {}", fen_parts.size());
        return;
    }

    stm_ = (fen_parts[1] == "w") ? WHITE : BLACK;

    for (const char c : fen_parts[2]) {
        switch (c) {
        case 'K':
            info.castling_rights.add_kingside(WHITE);
            break;
        case 'Q':
            info.castling_rights.add_queenside(WHITE);
            break;
        case 'k':
            info.castling_rights.add_kingside(BLACK);
            break;
        case 'q':
            info.castling_rights.add_queenside(BLACK);
            break;
        default:
            break;
        }
    }

    info.ep_sq = (fen_parts[3] == "-") ? NO_SQUARE : sq_from(fen_parts[3]);
    info.fifty_move_counter = std::stoi(fen_parts[4]);

    int sq = SQ_A8;
    for (const char c : fen_parts[0]) {
        if (isdigit(c))
            sq += c - '0';
        else if (c == '/')
            sq -= 16;
        else
            put_piece(Piece(PIECE_STR.find(c)), Square(sq++));
    }

    info.hash ^= zobrist::side();
    info.hash ^= zobrist::ep_sq(info.ep_sq);
    info.hash ^= zobrist::castling(info.castling_rights.hash_idx());

    init_movegen_info();
}

void Board::print() const {
    astra::print("\n +---+---+---+---+---+---+---+---+\n");

    for (int r = RANK_8; r >= RANK_1; r--) {
        for (int f = FILE_A; f <= FILE_H; f++)
            astra::print(" | {}", piece_at(make_square(static_cast<Rank>(r), static_cast<File>(f))));
        astra::print(" | {}\n +---+---+---+---+---+---+---+---+\n", 1 + r);
    }

    astra::print("   a   b   c   d   e   f   g   h\n");
    astra::print("\nFen: {}\n", fen());
    astra::print("Hash key: {:x}\n\n", state().hash);
}

std::string Board::fen() const {
    const StateInfo& info = state();
    std::ostringstream fen;

    for (int i = 56; i >= 0; i -= 8) {
        int empty = 0;
        for (int j = 0; j < 8; j++) {
            Piece pc = piece_at(static_cast<Square>(i + j));
            if (!valid_piece(pc)) {
                empty++;
            } else {
                if (empty)
                    fen << empty;
                fen << pc;
                empty = 0;
            }
        }

        if (empty != 0)
            fen << empty;
        if (i > 0)
            fen << '/';
    }

    std::string castling;
    if (info.castling_rights.kingside(WHITE))
        castling += "K";
    if (info.castling_rights.queenside(WHITE))
        castling += "Q";
    if (info.castling_rights.kingside(BLACK))
        castling += "k";
    if (info.castling_rights.queenside(BLACK))
        castling += "q";
    if (castling.empty())
        castling = "-";

    fen << (stm_ == WHITE ? " w " : " b ");
    fen << castling << " ";

    if (valid_sq(info.ep_sq))
        fen << info.ep_sq << " ";
    else
        fen << "- ";

    fen << info.fifty_move_counter << " ";
    fen << (ply_count() / 2 + 1);

    return fen.str();
}

nnue::DirtyPiece Board::make_move(Move move) {
    const Square from = move.from();
    const Square to = move.to();
    const Piece pc = piece_at(from);
    const PieceType pt = piece_type(pc);
    const Piece captured = capture_piece(move);

    nnue::DirtyPiece dirty_piece;

    assert(move);
    assert(valid_piece(pc));
    assert(piece_type(captured) != KING);
    assert(valid_piece(captured) == move.is_cap());

    StateInfo& info = states_.push();

    info.fifty_move_counter++;
    info.plies_from_null++;

    if (pt == PAWN || move.is_cap())
        info.fifty_move_counter = 0;

    if (valid_sq(info.ep_sq)) {
        info.hash ^= zobrist::ep_sq(info.ep_sq);
        info.ep_sq = NO_SQUARE;
    }

    if (move.is_castling()) {
        assert(pt == KING);

        auto [rook_from, rook_to] = castling_rook_sqs(stm_, to);
        assert(piece_at(rook_from) == make_piece(stm_, ROOK));

        move_piece(rook_from, rook_to);
        dirty_piece.remove(make_piece(stm_, ROOK), rook_from);
        dirty_piece.put(make_piece(stm_, ROOK), rook_to);
    }

    if (move.is_cap()) {
        Square cap_sq = move.is_ep() ? ep_sq(to) : to;
        remove_piece(cap_sq);
        dirty_piece.remove(captured, cap_sq);
    }

    move_piece(from, to);
    dirty_piece.move(pc, from, to);

    if (pt == PAWN) {
        Square new_ep_sq = ep_sq(to);
        if ((from ^ to) == 16 && (pawn_attacks_bb(stm_, new_ep_sq) & piece_bb<PAWN>(~stm_))) {
            info.ep_sq = new_ep_sq;
            info.hash ^= zobrist::ep_sq(new_ep_sq);
        } else if (move.is_prom()) {
            Piece prom_pc = make_piece(stm_, move.prom_type());
            assert(valid_piece(prom_pc));
            assert(piece_type(prom_pc) != PAWN);

            // remove pawn
            remove_piece(to);
            dirty_piece.move(pc, from, NO_SQUARE);

            put_piece(prom_pc, to);
            dirty_piece.put(prom_pc, to);
        }
    }

    if (info.castling_rights.on_castling_sq(from) || info.castling_rights.on_castling_sq(to))
        info.hash ^= info.castling_rights.update(from, to);

    info.hash ^= zobrist::side();

    stm_ = ~stm_;
    info.captured = captured;

    init_movegen_info();

    // update repetition info for upcoming repetition detection

    info.repetition = 0;
    int distance = std::min(info.fifty_move_counter, info.plies_from_null);

    if (distance >= 4) {
        StateInfo* prev = &states_(ply_count() - 2);
        for (int i = 4; i <= distance; i += 2) {
            prev -= 2;
            if (prev->hash == info.hash) {
                info.repetition = prev->repetition ? -i : i;
                break;
            }
        }
    }

    return dirty_piece;
}

void Board::undo_move(Move move) {
    const Square from = move.from();
    const Square to = move.to();
    const Piece captured = state().captured;

    assert(move);
    assert(valid_piece(piece_at(to)));
    assert(!valid_piece(piece_at(from)));

    stm_ = ~stm_;

    if (move.is_castling()) {
        auto [rook_to, rook_from] = castling_rook_sqs(stm_, to);
        move_piece(rook_from, rook_to);
    }

    if (move.is_prom()) {
        remove_piece(to);
        put_piece(make_piece(stm_, PAWN), to);
    }

    move_piece(to, from);

    if (move.is_cap()) {
        assert(valid_piece(captured));
        put_piece(captured, move.is_ep() ? ep_sq(to) : to);
    }

    states_.pop();
}

void Board::make_move() {
    StateInfo& info = states_.push();

    info.fifty_move_counter++;
    info.plies_from_null = 0;

    if (valid_sq(info.ep_sq)) {
        info.hash ^= zobrist::ep_sq(info.ep_sq);
        info.ep_sq = NO_SQUARE;
    }

    info.hash ^= zobrist::side();
    stm_ = ~stm_;

    init_movegen_info();
}

void Board::undo_move() {
    stm_ = ~stm_;
    states_.pop();
}

bool Board::is_legal(Move move) const {
    const Square from = move.from();
    const Square to = move.to();
    const Square ksq = king_sq();
    const Piece from_pc = piece_at(from);
    const Bitboard occ = occupancy();
    const Bitboard from_bb = sq_bb(from);

    assert(is_pseudo_legal(move));
    assert(piece_at(ksq) == make_piece(stm_, KING));

    if (move.is_ep()) {
        Square cap_sq = ep_sq(to);

        Bitboard new_occ = (occ ^ from_bb ^ sq_bb(cap_sq)) | sq_bb(to);

        assert(!valid_piece(piece_at(to)));
        assert(piece_at(cap_sq) == make_piece(~stm_, PAWN));

        Bitboard attackers = (attacks_bb<BISHOP>(ksq, new_occ) & diag_sliders(~stm_)) |
                             (attacks_bb<ROOK>(ksq, new_occ) & orth_sliders(~stm_));

        return !attackers;
    }

    if (move.is_castling()) {
        Direction step = (to == rel_sq(stm_, SQ_G1)) ? WEST : EAST;
        for (Square sq = to; sq != from; sq += step)
            if (attackers_to(~stm_, sq, occ))
                return false;
        return true;
    }

    if (piece_type(from_pc) == KING)
        return !attackers_to(~stm_, to, occ ^ from_bb);

    return !(state().blockers(stm_) & from_bb) || (line(from, to) & sq_bb(ksq));
}

bool Board::is_pseudo_legal(Move move) const {
    const StateInfo& info = state();

    const Square from = move.from();
    const Square to = move.to();
    const Square ksq = king_sq();
    const Square cap_sq = move.is_ep() ? ep_sq(to) : to;

    const Piece from_pc = piece_at(from);
    const Piece to_pc = piece_at(to);
    const Piece captured = piece_at(cap_sq);
    const PieceType pt = piece_type(from_pc);

    const Bitboard us_bb = occupancy(stm_);
    const Bitboard them_bb = occupancy(~stm_);
    const Bitboard occ = us_bb | them_bb;
    const Bitboard from_bb = sq_bb(from);

    const Bitboard target_bb = in_check() ? between_bb(ksq, lsb(info.checkers)) | info.checkers : ~0ULL;

    if (!move)
        return false;
    if (!valid_piece(from_pc))
        return false;
    if (piece_color(from_pc) != stm_)
        return false;
    if (sq_bb(to) & us_bb)
        return false;
    if (move.is_cap() != valid_piece(captured))
        return false;
    if (pop_count(info.checkers) > 1 && pt != KING)
        return false;

    if (in_check() && pt != KING && !(target_bb & sq_bb(cap_sq)))
        return false;

    if (move.is_castling()) {
        if (pt != KING || in_check())
            return false;
        if (to == rel_sq(stm_, SQ_G1))
            return info.castling_rights.kingside(stm_) && !(occ & kingside_castling_path_bb(stm_));
        else
            return info.castling_rights.queenside(stm_) && !(occ & queenside_castling_path_bb(stm_));
    }

    if (move.is_ep())
        if (pt != PAWN || info.ep_sq != to || valid_piece(to_pc) || captured != make_piece(~stm_, PAWN))
            return false;

    if (move.is_prom()) {
        if (pt != PAWN)
            return false;

        Bitboard targets = 0;
        // quiet promotions
        targets |= (stm_ == WHITE ? shift<NORTH>(from_bb) : shift<SOUTH>(from_bb)) & ~occ;
        // capture promotions
        targets |= (pawn_attacks_bb(stm_, from) & them_bb);
        // limit move targets based on checks
        targets &= target_bb;
        // limit move targets based on promotion rank
        targets &= rank_bb(rel_rank(stm_, RANK_8));

        return targets & sq_bb(to);
    }

    if (pt == PAWN) {
        // no promotion moves allowed here since we already handled them
        if (sq_bb(to) & (rank_bb(RANK_1) | rank_bb(RANK_8)))
            return false;

        if (!move.is_ep()) {
            const Direction up = (stm_ == WHITE ? NORTH : SOUTH);

            bool valid = false;
            // captures
            valid |= pawn_attacks_bb(stm_, from) & them_bb & sq_bb(to);
            // single push
            valid |= static_cast<Square>(from + up) == to && !valid_piece(to_pc);
            // double push
            valid |= static_cast<Square>(from + 2 * up) == to && //
                     rel_rank(stm_, RANK_2) == sq_rank(from) &&  //
                     !valid_piece(to_pc) && !valid_piece(piece_at(to - up));

            if (!valid)
                return false;
        }
    } else if (!(attacks_bb(pt, from, occ) & sq_bb(to))) {
        return false;
    }

    return true;
}

bool Board::gives_check(Move move) const {
    assert(move);
    assert(piece_color(piece_at(move.from())) == stm_);

    const Square from = move.from();
    const Square to = move.to();
    const Square nstm_ksq = king_sq(~stm_);
    const Bitboard occ = occupancy();

    if (check_squares(piece_type(piece_at(from))) & sq_bb(to))
        return true;

    if (state().blockers(~stm_) & from)
        return !(line(from, to) & piece_bb<KING>(~stm_)) || move.is_castling();

    if (move.is_prom()) {
        return attacks_bb(move.prom_type(), to, occ ^ from) & piece_bb<KING>(~stm_);
    } else if (move.is_ep()) {
        Square cap_sq = make_square(sq_rank(from), sq_file(to));
        Bitboard b = (occ ^ from ^ cap_sq) | to;

        return (attacks_bb<ROOK>(nstm_ksq, b) & orth_sliders(stm_)) |
               (attacks_bb<BISHOP>(nstm_ksq, b) & diag_sliders(stm_));
    } else if (move.is_castling()) {
        return check_squares(ROOK) & rel_sq(stm_, to > from ? SQ_F1 : SQ_D1);
    } else {
        return false;
    }
}

bool Board::upcoming_repetition(int ply) const {
    assert(ply > 0);

    const Bitboard occ = occupancy();
    const int total_plies = ply_count();
    const StateInfo& info = state();

    const int distance = std::min(info.fifty_move_counter, info.plies_from_null);
    if (distance < 3)
        return false;

    int offset = 1;

    Hash orig_key = info.hash;
    Hash other_key = orig_key ^ states_(total_plies - offset).hash ^ zobrist::side();

    for (int i = 3; i <= distance; i += 2) {
        offset++;

        int idx = total_plies - offset;
        other_key ^= states_(idx).hash ^ states_(idx - 1).hash ^ zobrist::side();

        offset++;
        idx = total_plies - offset;

        if (other_key != 0)
            continue;

        Hash move_key = orig_key ^ states_(idx).hash;
        int hash = cuckoo::h1(move_key);

        if (cuckoo::hash(hash) != move_key)
            hash = cuckoo::h2(move_key);
        if (cuckoo::hash(hash) != move_key)
            continue;

        Move move = cuckoo::move(hash);
        Square to = move.to();
        Bitboard between = between_bb(move.from(), to) ^ sq_bb(to);

        if (!(between & occ)) {
            if (ply > i)
                return true;
            if (states_(idx).repetition)
                return true;
        }
    }

    return false;
}

bool Board::see(Move move, int threshold) const {
    assert(move);

    if (move.is_prom() || move.is_ep() || move.is_castling())
        return true;

    const StateInfo& info = state();
    const Square from = move.from();
    const Square to = move.to();
    const PieceType attacker = piece_type(piece_at(from));
    const PieceType victim = piece_type(piece_at(to));

    assert(valid_piece_type(attacker));
    assert(valid_piece_type(victim) || move.is_quiet());
    assert(piece_color(piece_at(from)) == stm_);

    int swap = piece_values_see(victim) - threshold;
    if (swap < 0)
        return false;

    swap = piece_values_see(attacker) - swap;
    if (swap <= 0)
        return true;

    Bitboard new_occ = occupancy() ^ sq_bb(from) ^ sq_bb(to);
    Bitboard attackers = attackers_to(WHITE, to, new_occ) | attackers_to(BLACK, to, new_occ);

    const Bitboard diag = diag_sliders(WHITE) | diag_sliders(BLACK);
    const Bitboard orth = orth_sliders(WHITE) | orth_sliders(BLACK);

    int res = 1;

    Color curr_stm = side_to_move();

    Bitboard stm_attacker, bb;
    while (true) {
        curr_stm = ~curr_stm;
        attackers &= new_occ;

        if (!(stm_attacker = (attackers & occupancy(curr_stm))))
            break;

        if (info.pinners(~curr_stm) & new_occ) {
            stm_attacker &= ~info.blockers(curr_stm);
            if (!stm_attacker)
                break;
        }

        res ^= 1;

        if ((bb = stm_attacker & piece_bb<PAWN>())) {
            if ((swap = search::pawn_value_see - swap) < res)
                break;
            new_occ ^= (bb & -bb);
            attackers |= attacks_bb<BISHOP>(to, new_occ) & diag;
        } else if ((bb = stm_attacker & piece_bb<KNIGHT>())) {
            if ((swap = search::knight_value_see - swap) < res)
                break;
            new_occ ^= (bb & -bb);
        } else if ((bb = stm_attacker & piece_bb<BISHOP>())) {
            if ((swap = search::bishop_value_see - swap) < res)
                break;
            new_occ ^= (bb & -bb);
            attackers |= attacks_bb<BISHOP>(to, new_occ) & diag;
        } else if ((bb = stm_attacker & piece_bb<ROOK>())) {
            if ((swap = search::rook_value_see - swap) < res)
                break;
            new_occ ^= (bb & -bb);
            attackers |= attacks_bb<ROOK>(to, new_occ) & orth;
        } else if ((bb = stm_attacker & piece_bb<QUEEN>())) {
            swap = search::queen_value_see - swap;
            assert(swap >= res);
            new_occ ^= (bb & -bb);
            attackers |= (attacks_bb<BISHOP>(to, new_occ) & diag) | (attacks_bb<ROOK>(to, new_occ) & orth);
        } else {
            return (attackers & ~occupancy(curr_stm)) ? res ^ 1 : res;
        }
    }

    return bool(res);
}

void Board::perft(int depth) {
    if (depth < 1) {
        println("Invalid depth value: {}", depth);
        return;
    }

    auto perft = [&](int d, auto&& perft_ref) -> uint64_t {
        MoveList<Move> ml;
        ml.gen<GenType::LEGAL>(*this);

        if (d == 0)
            return 1;
        if (d <= 1)
            return ml.size();

        uint64_t nodes = 0;
        for (const auto& move : ml) {
            make_move(move);
            nodes += perft_ref(d - 1, perft_ref);
            undo_move(move);
        }
        return nodes;
    };

    auto start = std::chrono::high_resolution_clock::now();

    MoveList<Move> ml;
    ml.gen<GenType::LEGAL>(*this);

    uint64_t total_nodes = 0;
    for (const auto& move : ml) {
        make_move(move);
        uint64_t nodes = perft(depth - 1, perft);
        undo_move(move);
        total_nodes += nodes;
        println("{}: {}", move, nodes);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = end - start;
    double time_ms = diff.count();

    println("\nTotal nodes: {}", total_nodes);
    println("Total time : {} ms\n", time_ms);
}

Threats Board::threats() const {
    const Color them = ~stm_;

    Threats threats;

    // king must be excluded so we don't block the slider attacks
    const Bitboard occ = occupancy() ^ sq_bb(king_sq());

    const Bitboard pawns = piece_bb<PAWN>(them);
    threats(PAWN) = (them == WHITE) ? shift<NORTH_WEST>(pawns) | shift<NORTH_EAST>(pawns)
                                    : shift<SOUTH_WEST>(pawns) | shift<SOUTH_EAST>(pawns);

    for (PieceType pt : {KNIGHT, BISHOP, ROOK}) {
        threats(pt) = 0;

        Bitboard pc_bb = piece_bb(them, pt);
        while (pc_bb)
            threats(pt) |= attacks_bb(pt, pop_lsb(pc_bb), occ);
    }

    return threats;
}

// private function

void Board::init_movegen_info() {
    StateInfo& info = state();

    const Bitboard occ = occupancy();

    for (Color c : {WHITE, BLACK}) {
        info.blockers(c) = 0;
        info.pinners(~c) = 0;

        const Square ksq = king_sq(c);

        Bitboard cands = (attacks_bb<ROOK>(ksq) & orth_sliders(~c)) | (attacks_bb<BISHOP>(ksq) & diag_sliders(~c));

        const Bitboard new_occ = occ ^ cands;
        while (cands) {
            Square sq = pop_lsb(cands);
            Bitboard b = between_bb(ksq, sq) & new_occ;

            if (pop_count(b) == 1) {
                info.blockers(c) |= b;
                if (b & occupancy(c))
                    info.pinners(~c) |= sq_bb(sq);
            }
        }
    }

    info.checkers = attackers_to(~stm_, king_sq(), occ);

    Square nstm_ksq = king_sq(~stm_);
    info.check_squares(PAWN) = pawn_attacks_bb(~stm_, nstm_ksq);
    info.check_squares(KNIGHT) = attacks_bb<KNIGHT>(nstm_ksq);
    info.check_squares(BISHOP) = attacks_bb<BISHOP>(nstm_ksq, occ);
    info.check_squares(ROOK) = attacks_bb<ROOK>(nstm_ksq, occ);
    info.check_squares(QUEEN) = info.check_squares(BISHOP) | info.check_squares(ROOK);
}

} // namespace astra
