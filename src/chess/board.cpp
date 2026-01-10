#include <chrono>
#include <cstring>

#include "board.h"
#include "movegen.h"

namespace chess {

// Helper

std::pair<Square, Square> Board::castle_rook_sqs(Color c, Square to) {
    assert(to == rel_sq(c, g1) || to == rel_sq(c, c1));
    const bool ks = to == rel_sq(c, g1);
    return {rel_sq(c, ks ? h1 : a1), rel_sq(c, ks ? f1 : d1)};
}

int see_piece_values(PieceType pt) {
    // clang-format off
    switch(pt) {
        case PAWN:   return search::pawn_value_see;
        case KNIGHT: return search::knight_value_see;
        case BISHOP: return search::bishop_value_see;
        case ROOK:   return search::rook_value_see;
        case QUEEN:  return search::queen_value_see;
        default:     return 0;
    }
    // clang-format on
}

// Board

Board::Board(const std::string fen) {
    set_fen(fen);
}

Board& Board::operator=(const Board& other) {
    if (this != &other) {
        stm = other.stm;
        state_list = other.state_list;
        std::copy(std::begin(other.occ), std::end(other.occ), std::begin(occ));
        std::copy(std::begin(other.board), std::end(other.board), std::begin(board));
        std::copy(std::begin(other.pieces_bb), std::end(other.pieces_bb), std::begin(pieces_bb));
    }

    return *this;
}

void Board::set_fen(const std::string& fen) {
    std::memset(pieces_bb, 0, sizeof(pieces_bb));
    std::memset(occ, 0, sizeof(occ));

    for (auto& i : board)
        i = NO_PIECE;

    state_list.clear();
    StateInfo& info = state();

    info.hash = 0;
    info.pawn_hash = 0;
    info.minor_piece_hash = 0;
    info.non_pawn_hash[WHITE] = 0;
    info.non_pawn_hash[BLACK] = 0;

    std::vector<std::string> fen_parts = split(fen, ' ');
    if (fen_parts.size() != 6) {
        std::cerr << "Invalid fen string" << std::endl;
        return;
    }

    stm = (fen_parts[1] == "w") ? WHITE : BLACK;

    for (const char c : fen_parts[2]) {
        switch (c) {
            // clang-format off
            case 'K': info.castling_rights.add_kingside(WHITE); break;
            case 'Q': info.castling_rights.add_queenside(WHITE); break;
            case 'k': info.castling_rights.add_kingside(BLACK); break;
            case 'q': info.castling_rights.add_queenside(BLACK); break;
            default: break;
            // clang-format on
        }
    }

    info.ep_sq = (fen_parts[3] == "-") ? NO_SQUARE : sq_from(fen_parts[3]);
    info.fmr_counter = std::stoi(fen_parts[4]);

    // place pieces on the board
    int sq = a8;
    for (const char c : fen_parts[0]) {
        if (isdigit(c))
            sq += c - '0';
        else if (c == '/')
            sq -= 16;
        else
            put_piece(Piece(PIECE_STR.find(c)), Square(sq++));
    }

    // update hash
    info.hash ^= zobrist::side_hash();
    info.hash ^= zobrist::ep_hash(info.ep_sq);
    info.hash ^= zobrist::castling_hash(info.castling_rights.hash_idx());

    init_movegen_info();
}

void Board::print() const {
    std::cout << "\n +---+---+---+---+---+---+---+---+\n";

    for (int r = RANK_8; r >= RANK_1; --r) {
        for (int f = FILE_A; f <= FILE_H; ++f) {
            int s = (stm == WHITE) ? r * 8 + f : (7 - r) * 8 + (7 - f);
            std::cout << " | " << board[s];
        }

        std::cout << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";
    }

    std::cout << "   a   b   c   d   e   f   g   h\n";
    std::cout << "\nFen: " << fen() << std::endl;
    std::cout << "Hash key: " << std::hex << state().hash << std::dec << "\n\n";
}

std::string Board::fen() const {
    const StateInfo& info = state();
    std::ostringstream fen;

    // Build board representation
    for (int i = 56; i >= 0; i -= 8) {
        int empty = 0;
        for (int j = 0; j < 8; j++) {
            Piece pc = piece_at(Square(i + j));
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

    fen << (stm == WHITE ? " w " : " b ");
    fen << castling << " ";

    if (valid_sq(info.ep_sq))
        fen << info.ep_sq << " ";
    else
        fen << "- ";

    fen << info.fmr_counter << " ";
    fen << (ply_count() ? (ply_count() + 1) / 2 : 1);

    return fen.str();
}

nnue::DirtyPieces Board::make_move(Move move) {
    const Square from = move.from();
    const Square to = move.to();
    const Piece pc = piece_at(from);
    const PieceType pt = piece_type(pc);
    const Piece captured = captured_piece(move);

    nnue::DirtyPieces dirty_pieces;

    assert(move);
    assert(valid_piece(pc));
    assert(piece_type(captured) != KING);
    assert(valid_piece(captured) == move.is_cap());

    // update history
    StateInfo& info = state_list.increment();

    // update move counters
    info.fmr_counter++;
    info.plies_from_null++;

    // reset half move clock if pawn move or capture
    if (pt == PAWN || move.is_cap())
        info.fmr_counter = 0;

    // reset ep square if it exists
    if (valid_sq(info.ep_sq)) {
        info.hash ^= zobrist::ep_hash(info.ep_sq);
        info.ep_sq = NO_SQUARE;
    }

    if (move.is_castling()) {
        assert(pt == KING);

        auto [rook_from, rook_to] = castle_rook_sqs(stm, to);
        assert(piece_at(rook_from) == make_piece(stm, ROOK));

        move_piece(rook_from, rook_to);
        dirty_pieces.add(make_piece(stm, ROOK), rook_from, rook_to);
    }

    if (move.is_cap()) {
        Square cap_sq = move.is_ep() ? ep_sq(to) : to;
        remove_piece(cap_sq);
        dirty_pieces.add(captured, cap_sq, NO_SQUARE);
    }

    // move current piece
    move_piece(from, to);
    dirty_pieces.add(pc, from, to);

    if (pt == PAWN) {
        // set ep square if double push allows ep
        Square new_ep_sq = ep_sq(to);
        if ((from ^ to) == 16 && (pawn_attacks_bb(stm, new_ep_sq) & piece_bb(~stm, PAWN))) {
            info.ep_sq = new_ep_sq;
            info.hash ^= zobrist::ep_hash(new_ep_sq);
        } else if (move.is_prom()) {
            Piece prom_pc = make_piece(stm, move.prom_type());
            assert(valid_piece(prom_pc) && piece_type(prom_pc) != PAWN);

            dirty_pieces.pop(); // remove pawn from dirty pieces

            // remove pawn
            remove_piece(to);
            // pawn in accumulator is still in 'from' square
            dirty_pieces.add(make_piece(stm, PAWN), from, NO_SQUARE);

            // add promoted piece and
            put_piece(prom_pc, to);
            dirty_pieces.add(prom_pc, NO_SQUARE, to);
        }
    }

    // update castling rights if king/rook moves or if one of the rooks get captured
    if (info.castling_rights.on_castling_sq(from) || info.castling_rights.on_castling_sq(to)) {
        // remove old castling rights from hash
        info.hash ^= zobrist::castling_hash(info.castling_rights.hash_idx());
        info.castling_rights.update(from, to);
        // add new castling rights to hash
        info.hash ^= zobrist::castling_hash(info.castling_rights.hash_idx());
    }

    // update hash
    info.hash ^= zobrist::side_hash();

    stm = ~stm;
    info.captured = captured;

    init_movegen_info();

    // update repetition info for upcoming repetition detection

    info.repetition = 0;
    int distance = std::min(info.fmr_counter, info.plies_from_null);

    if (distance >= 4) {
        StateInfo* prev = &state_list[ply_count() - 2];
        for (int i = 4; i <= distance; i += 2) {
            prev -= 2;
            if (prev->hash == info.hash) {
                info.repetition = prev->repetition ? -i : i;
                break;
            }
        }
    }

    return dirty_pieces;
}

void Board::undo_move(Move move) {
    const Square from = move.from();
    const Square to = move.to();
    const Piece captured = state().captured;

    assert(move);
    assert(valid_piece(piece_at(to)));
    assert(!valid_piece(piece_at(from)));

    stm = ~stm;

    if (move.is_castling()) {
        auto [rook_to, rook_from] = castle_rook_sqs(stm, to);
        move_piece(rook_from, rook_to);
    }

    if (move.is_prom()) {
        remove_piece(to);
        put_piece(make_piece(stm, PAWN), to);
    }

    move_piece(to, from);

    if (move.is_cap()) {
        assert(valid_piece(captured));
        put_piece(captured, move.is_ep() ? ep_sq(to) : to);
    }

    state_list.decrement();
}

void Board::make_nullmove() {
    StateInfo& info = state_list.increment();

    info.fmr_counter++;
    info.plies_from_null = 0;

    if (valid_sq(info.ep_sq)) {
        info.hash ^= zobrist::ep_hash(info.ep_sq);
        info.ep_sq = NO_SQUARE;
    }

    info.hash ^= zobrist::side_hash();
    stm = ~stm;

    init_movegen_info();
}

void Board::undo_nullmove() {
    stm = ~stm;
    state_list.decrement();
}

void Board::perft(int depth) {
    if (depth < 1) {
        std::cout << "Invalid depth value.\n";
        return;
    }

    auto perft = [&](int d, auto&& perft_ref) -> U64 {
        MoveList<Move> ml;
        ml.gen<ADD_LEGALS>(*this);

        if (d == 0)
            return 1;
        if (d <= 1)
            return ml.size();

        U64 nodes = 0;
        for (const auto& move : ml) {
            make_move(move);
            nodes += perft_ref(d - 1, perft_ref);
            undo_move(move);
        }
        return nodes;
    };

    std::cout << "\nPerft test at depth " << depth << ":\n\n";
    auto start = std::chrono::high_resolution_clock::now();

    MoveList<Move> ml;
    ml.gen<ADD_LEGALS>(*this);

    U64 total_nodes = 0;
    for (const auto& move : ml) {
        make_move(move);
        U64 nodes = perft(depth - 1, perft);
        undo_move(move);

        total_nodes += nodes;
        std::cout << move << ": " << nodes << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> diff = end - start;
    double time_ms = diff.count();

    std::cout << "\nTotal nodes: " << total_nodes << std::endl;
    std::cout << "Total time : " << time_ms << " ms\n\n";
}

bool Board::legal(Move move) const {
    const Square from = move.from();
    const Square to = move.to();
    const Square ksq = king_sq();
    const Piece from_pc = piece_at(from);
    const U64 occ = occupancy();
    const U64 from_bb = sq_bb(from);

    assert(pseudo_legal(move));
    assert(piece_at(ksq) == make_piece(stm, KING));

    if (move.is_ep()) {
        Square cap_sq = ep_sq(to);

        // update occupancy as if the ep capture has occurred
        U64 new_occ = (occ ^ from_bb ^ sq_bb(cap_sq)) | sq_bb(to);

        assert(!valid_piece(piece_at(to)));
        assert(piece_at(cap_sq) == make_piece(~stm, PAWN));

        U64 attackers = (attacks_bb<BISHOP>(ksq, new_occ) & diag_sliders(~stm)) |
                        (attacks_bb<ROOK>(ksq, new_occ) & orth_sliders(~stm));

        // only legal if no discovered check occurs after ep capture
        return !attackers;
    }

    if (move.is_castling()) {
        Direction step = (to == rel_sq(stm, g1)) ? WEST : EAST;
        for (Square sq = to; sq != from; sq += step)
            if (attackers_to(~stm, sq, occ))
                return false; // squares passed by the king cannot be attacked
        return true;
    }

    if (piece_type(from_pc) == KING)
        return !attackers_to(~stm, to, occ ^ from_bb); // king cannot move to checking squares

    // only legal if not pinned or moving in the direction of the pin
    return !(state().blockers[stm] & from_bb) || (line(from, to) & sq_bb(ksq));
}

bool Board::pseudo_legal(Move move) const {
    const StateInfo& info = state();

    const Square from = move.from();
    const Square to = move.to();
    const Square ksq = king_sq();
    const Square cap_sq = move.is_ep() ? ep_sq(to) : to;

    const Piece from_pc = piece_at(from);
    const Piece to_pc = piece_at(to);
    const Piece captured = piece_at(cap_sq);
    const PieceType pt = piece_type(from_pc);

    const U64 us_bb = occupancy(stm);
    const U64 them_bb = occupancy(~stm);
    const U64 occ = us_bb | them_bb;
    const U64 from_bb = sq_bb(from);

    const U64 target_bb = in_check() ? between_bb(ksq, lsb(info.checkers)) | info.checkers : ~0ULL;

    // no and null moves are not pseudo legal
    if (!move)
        return false;
    // moving piece cannot capture its own pieces
    if (sq_bb(to) & us_bb)
        return false;
    // moving piece must be valid
    if (!valid_piece(from_pc))
        return false;
    // moving piece must match stm
    if (piece_color(from_pc) != stm)
        return false;
    // if move is quiet, then target square must be empty
    if (!move.is_cap() && valid_piece(to_pc))
        return false;
    // if move is a capture, then captured piece must be valid
    if (move.is_cap() && !valid_piece(captured))
        return false;
    // only king can move if there are multiple checkers
    if (pop_count(info.checkers) > 1 && pt != KING)
        return false;

    // if move can't capture/block the checker, then it's not pseudo legal
    if (in_check() && pt != KING && !(target_bb & sq_bb(cap_sq)))
        return false;

    if (move.is_castling()) {
        // only king can castle while not in check
        if (pt != KING || in_check())
            return false;
        // short castling
        if (to == rel_sq(stm, g1))
            return info.castling_rights.kingside(stm) && !(occ & ks_castle_path_mask(stm));
        // long castling
        else
            return info.castling_rights.queenside(stm) && !(occ & qs_castle_path_mask(stm));
        // if short/long castling condition is not met, then it's not pseudo legal
        return false;
    }

    if (move.is_ep()) {
        if (pt != PAWN                            // moving piece must be a pawn
            || info.ep_sq != to                   // ep square must match
            || valid_piece(to_pc)                 // target square must be empty
            || captured != make_piece(~stm, PAWN) // captured piece must be opponent's pawn
        ) {
            return false;
        }
    }

    if (move.is_prom()) {
        if (pt != PAWN) // only pawns can promote
            return false;

        U64 targets = 0;
        // quiet promotions
        targets |= (stm == WHITE ? shift<NORTH>(from_bb) : shift<SOUTH>(from_bb)) & ~occ;
        // capture promotions
        targets |= (pawn_attacks_bb(stm, from) & them_bb);
        // limit move targets based on checks
        targets &= target_bb;
        // limit move targets based on promotion rank
        targets &= rank_mask(rel_rank(stm, RANK_8));

        // only pseudo legal, if target range is reachable
        return targets & sq_bb(to);
    }

    if (pt == PAWN) {
        // no promotion moves allowed here since we already handled them
        if (sq_bb(to) & (rank_mask(RANK_1) | rank_mask(RANK_8)))
            return false;

        if (!move.is_ep()) {
            const Direction up = (stm == WHITE ? NORTH : SOUTH);

            bool valid = false;
            // captures
            valid |= pawn_attacks_bb(stm, from) & them_bb & sq_bb(to);
            // single push
            valid |= Square(from + up) == to && !valid_piece(to_pc);
            // double push
            valid |= Square(from + 2 * up) == to &&            //
                     rel_rank(stm, RANK_2) == sq_rank(from) && //
                     (to_pc + piece_at(to - up)) == 2 * NO_PIECE;

            // if none of the conditions above are met, then it's not pseudo legal
            if (!valid)
                return false;
        }
    }
    // if a non pawn piece's target range isn't reachable, then it's not pseudo legal
    else if (!(attacks_bb(pt, from, occ) & sq_bb(to))) {
        return false;
    }

    return true;
}

bool Board::gives_check(Move move) const {
    assert(move);
    assert(piece_color(piece_at(move.from())) == stm);

    const Square from = move.from();
    const Square to = move.to();
    const Square nstm_ksq = king_sq(~stm);
    const U64 occ = occupancy();

    if (check_squares(piece_type(piece_at(from))) & sq_bb(to))
        return true;

    if (state().blockers[~stm] & from)
        return !(line(from, to) & piece_bb(~stm, KING)) || move.is_castling();

    if (move.is_prom()) {
        return attacks_bb(move.prom_type(), to, occ ^ from) & piece_bb(~stm, KING);
    } else if (move.is_ep()) {
        Square cap_sq = make_square(sq_rank(from), sq_file(to));
        U64 b = (occ ^ from ^ cap_sq) | to;

        return (attacks_bb<ROOK>(nstm_ksq, b) & orth_sliders(stm)) |
               (attacks_bb<BISHOP>(nstm_ksq, b) & diag_sliders(stm));
    } else if (move.is_castling()) {
        return check_squares(ROOK) & rel_sq(stm, to > from ? f1 : d1);
    } else {
        return false;
    }
}

bool Board::upcoming_repetition(int ply) const {
    assert(ply > 0);

    const U64 occ = occupancy();
    const int total_plies = ply_count();
    const StateInfo& info = state();

    const int distance = std::min(info.fmr_counter, info.plies_from_null);
    if (distance < 3)
        return false;

    int offset = 1;

    U64 orig_key = info.hash;
    U64 other_key = orig_key ^ state_list[total_plies - offset].hash ^ zobrist::side_hash();

    for (int i = 3; i <= distance; i += 2) {
        offset++;

        int idx = total_plies - offset;
        other_key ^= state_list[idx].hash ^ state_list[idx - 1].hash ^ zobrist::side_hash();

        offset++;
        idx = total_plies - offset;

        if (other_key != 0)
            continue;

        U64 move_key = orig_key ^ state_list[idx].hash;
        int hash = cuckoo::cuckoo_h1(move_key);

        if (cuckoo::hash(hash) != move_key)
            hash = cuckoo::cuckoo_h2(move_key);
        if (cuckoo::hash(hash) != move_key)
            continue;

        Move move = cuckoo::move(hash);
        Square to = move.to();
        U64 between = between_bb(move.from(), to) ^ sq_bb(to);

        if (!(between & occ)) {
            if (ply > i)
                return true;
            if (state_list[idx].repetition)
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
    assert(piece_color(piece_at(from)) == stm);

    int swap = see_piece_values(victim) - threshold;
    if (swap < 0)
        return false;

    swap = see_piece_values(attacker) - swap;
    if (swap <= 0)
        return true;

    U64 new_occ = occupancy() ^ sq_bb(from) ^ sq_bb(to);
    U64 attackers = attackers_to(WHITE, to, new_occ) | attackers_to(BLACK, to, new_occ);

    const U64 diag = diag_sliders(WHITE) | diag_sliders(BLACK);
    const U64 orth = orth_sliders(WHITE) | orth_sliders(BLACK);

    int res = 1;

    Color curr_stm = side_to_move();

    U64 stm_attacker, bb;
    while (true) {
        curr_stm = ~curr_stm;
        attackers &= new_occ;

        if (!(stm_attacker = (attackers & occupancy(curr_stm))))
            break;

        if (info.pinners[~stm] & new_occ) {
            stm_attacker &= ~info.blockers[stm];
            if (!stm_attacker)
                break;
        }

        res ^= 1;

        if ((bb = stm_attacker & piece_bb(PAWN))) {
            if ((swap = see_piece_values(PAWN) - swap) < res)
                break;
            new_occ ^= (bb & -bb);
            attackers |= attacks_bb<BISHOP>(to, new_occ) & diag;
        } else if ((bb = stm_attacker & piece_bb(KNIGHT))) {
            if ((swap = see_piece_values(KNIGHT) - swap) < res)
                break;
            new_occ ^= (bb & -bb);
        } else if ((bb = stm_attacker & piece_bb(BISHOP))) {
            if ((swap = see_piece_values(BISHOP) - swap) < res)
                break;
            new_occ ^= (bb & -bb);
            attackers |= attacks_bb<BISHOP>(to, new_occ) & diag;
        } else if ((bb = stm_attacker & piece_bb(ROOK))) {
            if ((swap = see_piece_values(ROOK) - swap) < res)
                break;
            new_occ ^= (bb & -bb);
            attackers |= attacks_bb<ROOK>(to, new_occ) & orth;
        } else if ((bb = stm_attacker & piece_bb(QUEEN))) {
            swap = see_piece_values(QUEEN) - swap;
            assert(swap >= res);
            new_occ ^= (bb & -bb);
            attackers |= (attacks_bb<BISHOP>(to, new_occ) & diag) | (attacks_bb<ROOK>(to, new_occ) & orth);
        } else {
            return (attackers & ~occupancy(curr_stm)) ? res ^ 1 : res;
        }
    }

    return bool(res);
}

Threats Board::threats() const {
    const Color them = ~stm;

    Threats threats;

    // king must be excluded so we don't block the slider attacks
    const U64 occ = occupancy() ^ sq_bb(king_sq());

    const U64 pawns = piece_bb(them, PAWN);
    threats[PAWN] = (them == WHITE) ? shift<NORTH_WEST>(pawns) | shift<NORTH_EAST>(pawns)
                                    : shift<SOUTH_WEST>(pawns) | shift<SOUTH_EAST>(pawns);

    for (PieceType pt : {KNIGHT, BISHOP, ROOK}) {
        threats[pt] = 0;

        U64 pc_bb = piece_bb(them, pt);
        while (pc_bb)
            threats[pt] |= attacks_bb(pt, pop_lsb(pc_bb), occ);
    }

    return threats;
}

// private function

void Board::init_movegen_info() {
    StateInfo& info = state();

    const U64 occ = occupancy();

    for (Color c : {WHITE, BLACK}) {
        info.blockers[c] = 0;
        info.pinners[~c] = 0;

        const Square ksq = king_sq(c);

        // potential enemy bishop, rook and queen attacks at our king
        U64 cands = (attacks_bb<ROOK>(ksq) & orth_sliders(~c)) | (attacks_bb<BISHOP>(ksq) & diag_sliders(~c));

        const U64 new_occ = occ ^ cands;
        while (cands) {
            Square sq = pop_lsb(cands);
            U64 b = between_bb(ksq, sq) & new_occ;

            if (pop_count(b) == 1) {
                info.blockers[c] |= b;
                if (b & occupancy(c))
                    info.pinners[~c] |= sq_bb(sq);
            }
        }
    }

    info.checkers = attackers_to(~stm, king_sq(), occ);

    Square nstm_ksq = king_sq(~stm);

    info.check_squares[PAWN] = pawn_attacks_bb(~stm, nstm_ksq);
    info.check_squares[KNIGHT] = attacks_bb<KNIGHT>(nstm_ksq);
    info.check_squares[BISHOP] = attacks_bb<BISHOP>(nstm_ksq, occ);
    info.check_squares[ROOK] = attacks_bb<ROOK>(nstm_ksq, occ);
    info.check_squares[QUEEN] = info.check_squares[BISHOP] | info.check_squares[ROOK];
}

} // namespace chess
