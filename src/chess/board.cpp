#include <chrono>
#include <cstring>

#include "board.h"
#include "movegen.h"

namespace Chess {

// helper

// represent castle rights correctly in fen notation
bool castle_notation_helper(const std::ostringstream &fen_stream) {
    const std::string fen = fen_stream.str();
    const std::string rights = fen.substr(fen.find(' ') + 1);
    return rights.find_first_of("kqKQ") != std::string::npos;
}

std::pair<Square, Square> Board::get_castle_rook_sqs(Color c, Square to) {
    assert(valid_color(c));
    assert(to == rel_sq(c, g1) || to == rel_sq(c, c1));

    const bool ks = to == rel_sq(c, g1);

    return {rel_sq(c, ks ? h1 : a1), rel_sq(c, ks ? f1 : d1)};
}

int get_see_piece_value(PieceType pt) {
    switch(pt) {
        // clang-format off
        case PAWN:   return Engine::pawn_value_see;
        case KNIGHT: return Engine::knight_value_see;
        case BISHOP: return Engine::bishop_value_see;
        case ROOK:   return Engine::rook_value_see;
        case QUEEN:  return Engine::queen_value_see;
        default:     return 0;
        // clang-format on
    }
}

// class board

Board::Board(const std::string &fen) {
    set_fen(fen);
}

Board &Board::operator=(const Board &other) {
    if(this != &other) {
        stm = other.stm;
        curr_ply = other.curr_ply;
        states = other.states;

        std::copy(std::begin(other.board), std::end(other.board), std::begin(board));
        std::copy(std::begin(other.piece_bb), std::end(other.piece_bb), std::begin(piece_bb));
    }

    return *this;
}

void Board::set_fen(const std::string &fen) {
    curr_ply = 0;
    std::memset(piece_bb, 0, sizeof(piece_bb));

    for(auto &i : board)
        i = NO_PIECE;

    states[0] = StateInfo();
    StateInfo &info = states[0];

    std::vector<std::string> fen_parts = split(fen, ' ');
    if(fen_parts.size() != 6) {
        std::cerr << "Invalid fen string" << std::endl;
        return;
    }

    stm = (fen_parts[1] == "w") ? WHITE : BLACK;

    for(const char c : fen_parts[2]) {
        switch(c) {
            // clang-format off
            case 'K': info.castle_rights.add_ks(WHITE); break;
            case 'Q': info.castle_rights.add_qs(WHITE); break;
            case 'k': info.castle_rights.add_ks(BLACK); break;
            case 'q': info.castle_rights.add_qs(BLACK); break;
            default: break;
            // clang-format on
        }
    }

    info.ep_sq = (fen_parts[3] == "-") ? NO_SQUARE : sq_from(fen_parts[3]);
    info.fmr_counter = std::stoi(fen_parts[4]);

    // place pieces on the board
    int sqr = a8;
    for(const char c : fen_parts[0]) {
        if(isdigit(c))
            sqr += c - '0';
        else if(c == '/')
            sqr -= 16;
        else
            put_piece(Piece(PIECE_STR.find(c)), Square(sqr++));
    }

    // update hash
    info.hash ^= Zobrist::get_side();
    info.hash ^= Zobrist::get_ep(info.ep_sq);
    info.hash ^= Zobrist::get_castle(info.castle_rights.get_hash_idx());

    init_movegen_info();
}

void Board::print() const {
    std::cout << "\n +---+---+---+---+---+---+---+---+\n";

    for(int r = RANK_8; r >= RANK_1; --r) {
        for(int f = FILE_A; f <= FILE_H; ++f) {
            int s = (stm == WHITE) ? r * 8 + f : (7 - r) * 8 + (7 - f);
            std::cout << " | " << board[s];
        }

        std::cout << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";
    }

    std::cout << "   a   b   c   d   e   f   g   h\n";
    std::cout << "\nFen: " << get_fen() << std::endl;
    std::cout << "Hash key: " << std::hex << get_state().hash << std::dec << "\n\n";
}

std::string Board::get_fen() const {
    const StateInfo &info = get_state();
    std::ostringstream fen;

    for(int i = 56; i >= 0; i -= 8) {
        int empty = 0;

        for(int j = 0; j < 8; j++) {
            Piece p = board[i + j];
            if(!valid_piece(p))
                empty++;
            else {
                fen << (!empty ? "" : std::to_string(empty)) << p;
                empty = 0;
            }
        }

        if(empty != 0)
            fen << empty;
        if(i > 0)
            fen << '/';
    }

    std::ostringstream oss;
    oss << info.ep_sq;

    fen << (stm == WHITE ? " w " : " b ")             //
        << (info.castle_rights.ks(WHITE) ? "K" : "")  //
        << (info.castle_rights.qs(WHITE) ? "Q" : "")  //
        << (info.castle_rights.ks(BLACK) ? "k" : "")  //
        << (info.castle_rights.qs(BLACK) ? "q" : "")  //
        << (castle_notation_helper(fen) ? " " : "- ") //
        << (valid_sq(info.ep_sq) ? oss.str() : "-")   //
        << " " << info.fmr_counter                    //
        << " " << (!curr_ply ? 1 : (curr_ply + 1) / 2);

    return fen.str();
}

NNUE::DirtyPieces Board::make_move(const Move &move) {
    const Square from = move.from();
    const Square to = move.to();
    const Piece pc = piece_at(from);
    const PieceType pt = piece_type(pc);
    const Piece captured = move.is_ep() ? make_piece(~stm, PAWN) : piece_at(to);

    NNUE::DirtyPieces dirty_pieces;

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

    if(move.is_castling()) {
        assert(pt == KING);

        auto [rook_from, rook_to] = get_castle_rook_sqs(stm, to);
        assert(piece_at(rook_from) == make_piece(stm, ROOK));

        move_piece(rook_from, rook_to);
        dirty_pieces.add(make_piece(stm, ROOK), rook_from, rook_to);
    }

    if(move.is_cap()) {
        Square cap_sq = move.is_ep() ? Square(to ^ 8) : to;
        remove_piece(cap_sq);
        dirty_pieces.add(captured, cap_sq, NO_SQUARE);
    }

    // move current piece
    move_piece(from, to);
    dirty_pieces.add(pc, from, to);

    if(pt == PAWN) {
        // set ep square if double push can be captured by enemy pawn
        auto ep_sq = Square(to ^ 8);
        if((from ^ to) == 16 && (get_pawn_attacks(stm, ep_sq) & get_piece_bb<PAWN>(~stm))) {
            info.ep_sq = ep_sq;
            info.hash ^= Zobrist::get_ep(ep_sq); // add ep square to hash
        } else if(move.is_prom()) {
            PieceType prom_t = move.prom_type();
            Piece prom_pc = make_piece(stm, prom_t);

            assert(prom_t != PAWN);
            assert(valid_piece(prom_pc));

            // add promoted piece and remove pawn
            remove_piece(to);
            dirty_pieces.add(make_piece(stm, PAWN), to, NO_SQUARE);

            put_piece(prom_pc, to);
            dirty_pieces.add(prom_pc, NO_SQUARE, to);
        }
    }

    // update castling rights if king/rook moves or if one of the rooks get captured
    if(info.castle_rights.on_castle_sq(from) || info.castle_rights.on_castle_sq(to)) {
        // remove old castling rights from hash
        info.hash ^= Zobrist::get_castle(info.castle_rights.get_hash_idx());
        info.castle_rights.update(sq_bb(from), sq_bb(to));
        // add new castling rights to hash
        info.hash ^= Zobrist::get_castle(info.castle_rights.get_hash_idx());
    }

    // update hash
    info.hash ^= Zobrist::get_side();

    stm = ~stm;
    info.captured = captured;

    init_movegen_info();

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

    return dirty_pieces;
}

void Board::undo_move(const Move &move) {
    const Square from = move.from();
    const Square to = move.to();
    const Piece captured = get_state().captured;

    assert(move);
    assert(valid_piece(piece_at(to)));
    assert(!valid_piece(piece_at(from)));

    stm = ~stm;

    if(move.is_prom()) {
        remove_piece(to);
        put_piece(make_piece(stm, PAWN), to);
    }

    move_piece(to, from);

    if(move.is_castling()) {
        auto [rook_to, rook_from] = get_castle_rook_sqs(stm, to);
        move_piece(rook_from, rook_to);
    } else if(move.is_cap()) {
        assert(valid_piece(captured));
        Square cap_sqr = move.is_ep() ? Square(to ^ 8) : to;
        put_piece(captured, cap_sqr);
    }

    curr_ply--;
}

void Board::make_nullmove() {
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

    init_movegen_info();
}

void Board::undo_nullmove() {
    curr_ply--;
    stm = ~stm;
}

void Board::perft(int depth) {
    if(depth < 1) {
        std::cout << "Invalid depth value.\n";
        return;
    }

    auto perft = [&](int d, auto &&perft_ref) -> U64 {
        MoveList<> ml;
        ml.gen<ADD_LEGALS>(*this);

        if(d == 0)
            return 1;

        if(d <= 1)
            return ml.size();

        U64 nodes = 0;
        for(const Move &move : ml) {
            make_move(move);
            nodes += perft_ref(d - 1, perft_ref);
            undo_move(move);
        }
        return nodes;
    };

    std::cout << "\nPerft test at depth " << depth << ":\n\n";
    auto start = std::chrono::high_resolution_clock::now();

    MoveList<> ml;
    ml.gen<ADD_LEGALS>(*this);

    U64 total_nodes = 0;
    for(const Move &move : ml) {
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
    std::cout << "Total time: " << time_ms << "ms\n\n";
}

bool Board::is_legal(const Move &move) const {
    const Square from = move.from();
    const Square to = move.to();
    const Square ksq = get_king_sq(stm);
    const Piece from_pc = piece_at(from);

    const U64 occ = get_occupancy();

    assert(move);
    assert(valid_piece(from_pc));
    assert(piece_at(ksq) == make_piece(stm, KING));

    if(move.is_ep()) {
        Square cap_sq = Square(to ^ 8);

        // update occupancy as if the ep capture has occurred
        U64 new_occ = (occ ^ sq_bb(from) ^ sq_bb(cap_sq)) | sq_bb(to);

        assert(piece_at(cap_sq) == make_piece(~stm, PAWN));
        assert(!valid_piece(piece_at(to)));

        U64 attackers = (get_attacks<BISHOP>(ksq, new_occ) & get_diag_sliders(~stm)) |
                        (get_attacks<ROOK>(ksq, new_occ) & get_orth_sliders(~stm));

        // only legal if no discovered check occurs after ep capture
        return !attackers;
    }

    if(move.is_castling()) {
        Direction step = (to == rel_sq(stm, g1)) ? WEST : EAST;
        for(Square sq = to; sq != from; sq += step)
            if(attackers_to(~stm, sq, occ))
                return false; // squares passed by the king cannot be attacked
        return true;
    }

    if(piece_type(from_pc) == KING)
        // king cannot move to checking squares
        return !attackers_to(~stm, to, occ ^ sq_bb(from));

    // only legal if not pinned or moving in the direction of the pin
    return !(get_state().blockers[stm] & sq_bb(from)) || (line(from, to) & sq_bb(ksq));
}

bool Board::is_pseudo_legal(const Move &move) const {
    const StateInfo &info = get_state();

    const Square from = move.from();
    const Square to = move.to();
    const Square ksq = get_king_sq(stm);
    const Square cap_sq = move.is_ep() ? Square(to ^ 8) : to;

    const Piece from_pc = piece_at(from);
    const Piece to_pc = piece_at(to);
    const Piece captured = piece_at(cap_sq);

    const PieceType pt = piece_type(from_pc);

    const U64 us_bb = get_occupancy(stm);
    const U64 them_bb = get_occupancy(~stm);
    const U64 occ = us_bb | them_bb;

    if(!move)
        return false; // no and null moves are not pseudo legal

    if(!valid_piece(from_pc))
        return false; // moving piece must be valid

    if(sq_bb(to) & us_bb)
        return false; // moving piece cannot capture its own pieces

    if(piece_color(from_pc) != stm)
        return false; // moving piece must match stm

    if(!move.is_cap() && valid_piece(to_pc))
        return false; // if move is quiet, then target square must be empty

    if(move.is_cap() && !valid_piece(captured))
        return false; // if move is a capture, then captured piece must be valid

    if(pop_count(info.checkers) > 1 && pt != KING)
        return false; // only king can move if there are multiple checkers

    if(move.is_castling()) {
        if(pt != KING || info.checkers || !info.castle_rights.any(stm))
            return false; // only king can castle, if not in check and has castling rights

        // short castling
        if(to == rel_sq(stm, g1))
            return info.castle_rights.ks(stm) && !(occ & ks_castle_path_mask(stm));
        // long castling
        else
            return info.castle_rights.qs(stm) && !(occ & qs_castle_path_mask(stm));

        // if short/long castling condition is not met, then it's not pseudo legal
        return false;
    }

    if(move.is_ep()) {
        if(pt != PAWN                            // moving piece must be a pawn
           || info.ep_sq != to                   // ep square must match
           || valid_piece(to_pc)                 // target square must be empty
           || captured != make_piece(~stm, PAWN) // captured piece must be opponent's pawn
        ) {
            return false;
        }
    }

    if(move.is_prom()) {
        if(pt != PAWN) // only pawns can promote
            return false;

        U64 targets = 0;

        // quiet promotions
        targets |= (stm == WHITE ? shift<NORTH>(sq_bb(from)) : shift<SOUTH>(sq_bb(from))) & ~occ;
        // capture promotions
        targets |= (get_pawn_attacks(stm, from) & them_bb);
        // limit move targets based on checks
        targets &= info.checkers ? between_bb(ksq, lsb(info.checkers)) | info.checkers : -1ULL;
        // limit move targets based on promotion rank
        targets &= rank_mask(rel_rank(stm, RANK_8));

        // only pseudo legal, if target range is reachable
        return targets & sq_bb(to);
    }

    if(pt == PAWN) {
        if(sq_bb(to) & (rank_mask(RANK_1) | rank_mask(RANK_8)))
            return false; // no promotion moves allowed here since we already handled them

        if(!move.is_ep()) {
            const Direction up = (stm == WHITE ? NORTH : SOUTH);
            bool valid = false;

            // captures
            valid |= get_pawn_attacks(stm, from) & them_bb & sq_bb(to);
            // single push
            valid |= Square(from + up) == to && !valid_piece(to_pc);
            // double push
            valid |= Square(from + 2 * up) == to &&            //
                     rel_rank(stm, RANK_2) == sq_rank(from) && //
                     (to_pc + piece_at(to - up)) == 2 * NO_PIECE;

            if(!valid)
                return false; // if none of the conditions above are met, then it's not pseudo legal
        }
    }
    // if a non pawn piece's target range isn't reachable, then it's not pseudo legal
    else if(!(get_attacks(pt, from, occ) & sq_bb(to))) {
        return false;
    }

    // check for blockers/captures of the checker
    if(info.checkers && pt != KING) {
        const U64 target = between_bb(ksq, lsb(info.checkers)) | info.checkers;
        if(!(target & sq_bb(cap_sq)))
            return false; // if move can't capture/block the checker, then it's not pseudo legal
    }

    if(info.blockers[stm] & sq_bb(from) && !(line(from, to) & sq_bb(ksq)))
        return false; // if a pinned piece move causes a discovered check, then it's not pseudo legal

    return true;
}

bool Board::see(const Move &move, int threshold) const {
    assert(move);

    if(move.is_prom() || move.is_ep() || move.is_castling())
        return true;

    const StateInfo &info = get_state();
    const Square from = move.from();
    const Square to = move.to();
    const PieceType attacker = piece_type(piece_at(from));
    const PieceType victim = piece_type(piece_at(to));

    assert(valid_piece_type(attacker));
    assert(piece_color(piece_at(from)) == stm);

    int swap = get_see_piece_value(victim) - threshold;
    if(swap < 0)
        return false;

    swap = get_see_piece_value(attacker) - swap;

    if(swap <= 0)
        return true;

    U64 occ = get_occupancy() ^ sq_bb(from) ^ sq_bb(to);
    U64 attackers = attackers_to(WHITE, to, occ) | attackers_to(BLACK, to, occ);

    const U64 diag = get_diag_sliders(WHITE) | get_diag_sliders(BLACK);
    const U64 orth = get_orth_sliders(WHITE) | get_orth_sliders(BLACK);

    int res = 1;

    Color curr_stm = get_stm();

    U64 stm_attacker, bb;
    while(true) {
        curr_stm = ~curr_stm;
        attackers &= occ;

        if(!(stm_attacker = (attackers & get_occupancy(curr_stm))))
            break;

        if(info.pinners[~stm] & occ) {
            stm_attacker &= ~info.blockers[stm];
            if(!stm_attacker)
                break;
        }

        res ^= 1;

        if((bb = stm_attacker & get_piece_bb<PAWN>())) {
            if((swap = get_see_piece_value(PAWN) - swap) < res)
                break;
            occ ^= (bb & -bb);
            attackers |= get_attacks<BISHOP>(to, occ) & diag;
        } else if((bb = stm_attacker & get_piece_bb<KNIGHT>())) {
            if((swap = get_see_piece_value(KNIGHT) - swap) < res)
                break;
            occ ^= (bb & -bb);
        } else if((bb = stm_attacker & get_piece_bb<BISHOP>())) {
            if((swap = get_see_piece_value(BISHOP) - swap) < res)
                break;
            occ ^= (bb & -bb);
            attackers |= get_attacks<BISHOP>(to, occ) & diag;
        } else if((bb = stm_attacker & get_piece_bb<ROOK>())) {
            if((swap = get_see_piece_value(ROOK) - swap) < res)
                break;
            occ ^= (bb & -bb);
            attackers |= get_attacks<ROOK>(to, occ) & orth;
        } else if((bb = stm_attacker & get_piece_bb<QUEEN>())) {
            swap = get_see_piece_value(QUEEN) - swap;
            assert(swap >= res);
            occ ^= (bb & -bb);
            attackers |= (get_attacks<BISHOP>(to, occ) & diag) | (get_attacks<ROOK>(to, occ) & orth);
        } else {
            return (attackers & ~get_occupancy(curr_stm)) ? res ^ 1 : res;
        }
    }

    return bool(res);
}

bool Board::upcoming_repetition(int ply) {
    assert(ply > 0);

    const U64 occ = get_occupancy();
    const StateInfo &info = get_state();

    int distance = std::min(info.fmr_counter, info.plies_from_null);
    if(distance < 3)
        return false;

    int offset = 1;

    U64 orig_key = info.hash;
    U64 other_key = orig_key ^ states[curr_ply - offset].hash ^ Zobrist::get_side();

    for(int i = 3; i <= distance; i += 2) {
        offset++;
        int idx = curr_ply - offset;

        other_key ^= states[idx].hash ^ states[idx - 1].hash ^ Zobrist::get_side();

        offset++;
        idx = curr_ply - offset;

        if(other_key != 0)
            continue;

        U64 move_key = orig_key ^ states[idx].hash;
        int hash = Cuckoo::cuckoo_h1(move_key);

        if(Cuckoo::keys[hash] != move_key)
            hash = Cuckoo::cuckoo_h2(move_key);
        if(Cuckoo::keys[hash] != move_key)
            continue;

        Move move = Cuckoo::cuckoo_moves[hash];
        Square to = move.to();
        U64 between = between_bb(move.from(), to) ^ sq_bb(to);

        if(!(between & occ)) {
            if(ply > i)
                return true;
            if(states[idx].repetition)
                return true;
        }
    }

    return false;
}

Threats Board::get_threats() const {
    const Color them = ~stm;

    Threats threats;

    // king must be excluded so we don't block the slider attacks
    U64 occ = get_occupancy() ^ sq_bb(get_king_sq(stm));

    U64 pawns = get_piece_bb<PAWN>(them);
    threats[PAWN] = (them == WHITE) ? shift<NORTH_WEST>(pawns) | shift<NORTH_EAST>(pawns)
                                    : shift<SOUTH_WEST>(pawns) | shift<SOUTH_EAST>(pawns);

    for(PieceType pt : {KNIGHT, BISHOP, ROOK}) {
        threats[pt] = 0;

        U64 pc_bb = get_piece_bb(them, pt);
        while(pc_bb)
            threats[pt] |= get_attacks(pt, pop_lsb(pc_bb), occ);
    }

    return threats;
}

// private member

void Board::init_movegen_info() {
    StateInfo &info = get_state();

    auto helper = [&](Color c) {
        info.blockers[c] = 0;
        info.pinners[~c] = 0;

        const Square ksq = get_king_sq(c);

        // potential enemy bishop, rook and queen attacks at our king
        U64 cands = (get_attacks<ROOK>(ksq) & get_orth_sliders(~c)) | //
                    (get_attacks<BISHOP>(ksq) & get_diag_sliders(~c));

        const U64 occ = get_occupancy() ^ cands;
        while(cands) {
            Square sq = pop_lsb(cands);
            U64 b = between_bb(ksq, sq) & occ;

            if(pop_count(b) == 1) {
                info.blockers[c] |= b;
                if(b & get_occupancy(c))
                    info.pinners[~c] |= sq_bb(sq);
            }
        }
    };

    helper(WHITE);
    helper(BLACK);

    info.checkers = attackers_to(~stm, get_king_sq(stm), get_occupancy());
}

} // namespace Chess
