#include <cstring>

#include "board.h"

namespace Chess {

// helper

// represent castle rights correctly in fen notation
bool castle_notation_helper(const std::ostringstream &fen_stream) {
    const std::string fen = fen_stream.str();
    const std::string rights = fen.substr(fen.find(' ') + 1);
    return rights.find_first_of("kqKQ") != std::string::npos;
}

std::pair<Square, Square> get_castle_rook_squares(Color c, Square to) {
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

// class board

Board::Board(const std::string &fen, bool init_accum) {
    set_fen(fen, init_accum);
}

Board &Board::operator=(const Board &other) {
    if(this != &other) {
        stm = other.stm;
        curr_ply = other.curr_ply;
        states = other.states;
        accum_list.copy(other.accum_list, this);

        std::copy(std::begin(other.board), std::end(other.board), std::begin(board));
        std::copy(std::begin(other.piece_bb), std::end(other.piece_bb), std::begin(piece_bb));
    }

    return *this;
}

void Board::set_fen(const std::string &fen, bool init_accum) {
    curr_ply = 0;
    accum_list.set_board(this);
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
            case 'K': info.castle_rights.add_kingside(WHITE); break;
            case 'Q': info.castle_rights.add_queenside(WHITE); break;
            case 'k': info.castle_rights.add_kingside(BLACK); break;
            case 'q': info.castle_rights.add_queenside(BLACK); break;
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

    // initialize Zobrist hashes
    info.pawn_hash = Zobrist::get_pawn(*this);
    info.non_pawn_hash[WHITE] = Zobrist::get_nonpawn(*this, WHITE);
    info.non_pawn_hash[BLACK] = Zobrist::get_nonpawn(*this, BLACK);

    info.hash = info.pawn_hash ^ info.non_pawn_hash[WHITE] ^ info.non_pawn_hash[BLACK];

    info.hash ^= Zobrist::get_side();
    info.hash ^= Zobrist::get_ep(info.ep_sq);
    info.hash ^= Zobrist::get_castle(info.castle_rights.get_hash_idx());

    init_threats();
    init_slider_blockers();

    if(init_accum)
        reset_accum_list();
}

void Board::print() const {
    std::cout << "\n +---+---+---+---+---+---+---+---+\n";

    for(int r = RANK_8; r >= RANK_1; --r) {
        for(int f = FILE_A; f <= FILE_H; ++f) {
            int s;
            if(stm == WHITE)
                s = r * 8 + f;
            else
                s = (7 - r) * 8 + (7 - f);
            std::cout << " | " << board[s];
        }
        std::cout << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";
    }

    std::cout << "   a   b   c   d   e   f   g   h\n";
    std::cout << "\nFen: " << get_fen() << std::endl;
    std::cout << "Hash key: " << std::hex << states[curr_ply].hash << std::dec << "\n\n";
}

std::string Board::get_fen() const {
    const StateInfo &info = states[curr_ply];
    std::ostringstream fen;

    for(int i = 56; i >= 0; i -= 8) {
        int empty = 0;

        for(int j = 0; j < 8; j++) {
            Piece p = board[i + j];
            if(!valid_piece(p))
                empty++;
            else {
                fen << (empty == 0 ? "" : std::to_string(empty)) << p;
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

    fen << (stm == WHITE ? " w " : " b ")                   //
        << (info.castle_rights.kingside(WHITE) ? "K" : "")  //
        << (info.castle_rights.queenside(WHITE) ? "Q" : "") //
        << (info.castle_rights.kingside(BLACK) ? "k" : "")  //
        << (info.castle_rights.queenside(BLACK) ? "q" : "") //
        << (castle_notation_helper(fen) ? " " : "- ")       //
        << (valid_sq(info.ep_sq) ? oss.str() : "-")         //
        << " " << info.fmr_counter                          //
        << " " << (curr_ply == 0 ? 1 : (curr_ply + 1) / 2);

    return fen.str();
}

bool Board::is_legal(const Move &m) const {
    const Square from = m.from();
    const Square to = m.to();
    const Square ksq = king_sq(stm);
    const Piece from_pc = piece_at(from);

    assert(m);
    assert(valid_piece(from_pc));
    assert(piece_at(ksq) == make_piece(stm, KING));

    if(m.is_ep()) {
        Square cap_sq = Square(to ^ 8);
        U64 occ = (get_occupancy() ^ square_bb(from) ^ square_bb(cap_sq)) | square_bb(to);

        assert(piece_at(cap_sq) == make_piece(~stm, PAWN));
        assert(!valid_piece(piece_at(to)));

        U64 attackers = get_attacks(BISHOP, ksq, occ) //
                        & (get_piecebb(~stm, BISHOP) | get_piecebb(~stm, QUEEN));

        attackers |= get_attacks(ROOK, ksq, occ) //
                     & (get_piecebb(~stm, ROOK) | get_piecebb(~stm, QUEEN));

        // only legal if no discovered check occurs after ep capture
        return !attackers;
    }

    if(m.is_castling()) {
        Direction step = (to == rel_sq(stm, g1)) ? WEST : EAST;
        for(Square sq = to; sq != from; sq += step)
            if(attackers_to(~stm, sq, get_occupancy()))
                return false;

        return true;
    }

    // make sure king doesn't move to checking squares
    if(piece_type(from_pc) == KING)
        return !(attackers_to(~stm, to, get_occupancy() ^ square_bb(from)));

    // only legal if not pinned or moving in the direction of the pin
    return !(states[curr_ply].blockers[stm] & square_bb(from)) || (line(from, to) & square_bb(ksq));
}

bool Board::is_pseudolegal(const Move &m) const {
    const StateInfo &info = states[curr_ply];

    const Square from = m.from();
    const Square to = m.to();
    const Square ksq = king_sq(stm);

    const Piece from_pc = piece_at(from);
    const Piece to_pc = piece_at(to);
    const PieceType pt = piece_type(from_pc);

    const U64 us_bb = get_occupancy(stm);
    const U64 them_bb = get_occupancy(~stm);
    const U64 occ = us_bb | them_bb;

    if(!m)
        return false;
    if(!valid_piece(from_pc))
        return false;
    if(square_bb(to) & us_bb)
        return false;
    if(piece_color(from_pc) != stm)
        return false;
    if(!m.is_cap() && valid_piece(to_pc))
        return false;
    if(pop_count(info.checkers) > 1 && pt != KING)
        return false;
    if(pt == KING && (square_bb(to) & info.danger))
        return false;
    if(!m.is_ep() && m.is_cap() && !valid_piece(to_pc))
        return false;

    if(m.is_castling()) {
        if(pt != KING || info.checkers || !info.castle_rights.any(stm)) {
            return false;
        }

        // short castling
        U64 not_free = (occ | info.danger) & OO_BLOCKERS_MASK[stm];
        if(!not_free && info.castle_rights.kingside(stm) && to == rel_sq(stm, g1))
            return true;

        // long castling
        not_free = (occ | (info.danger & ~square_bb(rel_sq(stm, b1)))) & OOO_BLOCKERS_MASK[stm];
        if(!not_free && info.castle_rights.queenside(stm) && to == rel_sq(stm, c1))
            return true;

        // if short/long castling condition is not met, then it's not pseudo legal
        return false;
    }

    if(m.is_ep()) {
        if(pt != PAWN                                                    //
           || info.ep_sq != to                                           //
           || valid_piece(to_pc)                                         //
           || piece_at(Square(info.ep_sq ^ 8)) != make_piece(~stm, PAWN) //
        ) {
            return false;
        }
    }

    if(m.is_prom()) {
        if(pt != PAWN)
            return false;

        U64 up_bb = (stm == WHITE ? square_bb(from) << 8 : square_bb(from) >> 8) & ~occ;
        U64 attacks = up_bb | (get_pawn_attacks(stm, from) & them_bb);
        U64 targets = info.checkers ? between_bb(ksq, lsb(info.checkers)) | info.checkers : -1ULL;

        // only pseudo legal, if target range is reachable
        return (MASK_RANK[rel_rank(stm, RANK_8)] & attacks & targets) & square_bb(to);
    }

    if(pt == PAWN) {
        const Direction up = (stm == WHITE ? NORTH : SOUTH);

        // no promotion moves allowed here since we already handled them
        if(square_bb(to) & (MASK_RANK[RANK_1] | MASK_RANK[RANK_8]))
            return false;

        if(!m.is_ep()) {
            bool capture = get_pawn_attacks(stm, from) & them_bb & square_bb(to);

            bool singe_push = Square(from + up) == to && !valid_piece(to_pc);

            bool double_push = Square(from + 2 * up) == to &&            //
                               rel_rank(stm, RANK_2) == sq_rank(from) && //
                               (to_pc + piece_at(to - up)) == 2 * NO_PIECE;

            // if none of the conditions above are met, then it's not pseudo legal
            if(!capture && !singe_push && !double_push)
                return false;
        }
    }
    // if a non pawn piece's target range isn't reachable, then it's not pseudo legal
    else if(!(get_attacks(pt, from, occ) & square_bb(to))) {
        return false;
    }

    // check for blockers/captures of the checker
    if(info.checkers && pt != KING) {
        U64 target = between_bb(ksq, lsb(info.checkers)) | info.checkers;
        Square cap_sq = m.is_ep() ? Square(to ^ 8) : to;

        // if move can't capture/block the checker, then it's not pseudo legal
        if(!(target & square_bb(cap_sq)))
            return false;
    }

    return true;
}

bool Board::is_repetition(int ply) const {
    const StateInfo &info = states[curr_ply];
    const int distance = std::min(info.plies_from_null, info.fmr_counter);

    int rep = 0;
    for(int i = curr_ply - 4; i >= 0 && i >= curr_ply - distance; i -= 2) {
        if(states[i].hash != info.hash)
            continue;

        if(i > curr_ply - ply)
            return true;
        rep++;
        if(rep == 2)
            return true;
    }

    return false;
}

void Board::make_move(const Move &m, bool update_accum) {
    const Square from = m.from();
    const Square to = m.to();
    const Piece pc = piece_at(from);
    const PieceType pt = piece_type(pc);
    const Piece captured = m.is_ep() ? make_piece(~stm, PAWN) : piece_at(to);

    assert(m);
    assert(valid_piece(pc));
    assert(piece_type(captured) != KING);

    // update history
    curr_ply++;
    states[curr_ply] = StateInfo(states[curr_ply - 1]);
    StateInfo &info = states[curr_ply];

    // update move counters
    info.fmr_counter++;
    info.plies_from_null++;

    // reset half move clock if pawn move or capture
    if(pt == PAWN || valid_piece(captured))
        info.fmr_counter = 0;

    // reset ep square if it exists
    if(valid_sq(info.ep_sq)) {
        info.hash ^= Zobrist::get_ep(info.ep_sq); // remove ep square from hash
        info.ep_sq = NO_SQUARE;
    }

    // increment accum
    if(update_accum)
        accum_list.push();

    if(m.is_castling()) {
        assert(pt == KING);

        auto [rook_from, rook_to] = get_castle_rook_squares(stm, to);
        Piece rook = piece_at(rook_from);

        assert(rook == make_piece(stm, ROOK));

        move_piece(rook_from, rook_to, update_accum);

        // update hash of moving rook
        info.hash ^= Zobrist::get_psq(rook, rook_from) ^ Zobrist::get_psq(rook, rook_to);
        info.non_pawn_hash[stm] ^= Zobrist::get_psq(rook, rook_from) ^ Zobrist::get_psq(rook, rook_to);
    }

    if(valid_piece(captured)) {
        Square cap_sq = m.is_ep() ? Square(to ^ 8) : to;

        remove_piece(cap_sq, update_accum);

        // update hash
        info.hash ^= Zobrist::get_psq(captured, cap_sq); // remove captured piece from hash
        if(piece_type(captured) == PAWN)
            info.pawn_hash ^= Zobrist::get_psq(captured, cap_sq);
        else
            info.non_pawn_hash[~stm] ^= Zobrist::get_psq(captured, cap_sq);
    }

    // move current piece
    move_piece(from, to, update_accum);

    // update hash of moving piece
    info.hash ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);
    if(pt == PAWN)
        info.pawn_hash ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);
    else
        info.non_pawn_hash[stm] ^= Zobrist::get_psq(pc, from) ^ Zobrist::get_psq(pc, to);

    if(pt == PAWN) {
        // set ep square if double push can be captured by enemy pawn
        auto ep_sq = Square(to ^ 8);
        if((from ^ to) == 16 && (get_pawn_attacks(stm, ep_sq) & get_piecebb(~stm, PAWN))) {
            info.ep_sq = ep_sq;
            info.hash ^= Zobrist::get_ep(ep_sq); // add ep square to hash
        } else if(m.is_prom()) {
            PieceType prom_t = m.prom_type();
            Piece prom_pc = make_piece(stm, prom_t);

            assert(prom_t != PAWN);
            assert(valid_piece(prom_pc));

            // add promoted piece and remove pawn
            remove_piece(to, update_accum);
            put_piece(prom_pc, to, update_accum);

            // update hash of promoting piece
            info.hash ^= Zobrist::get_psq(pc, to) ^ Zobrist::get_psq(prom_pc, to);
            info.pawn_hash ^= Zobrist::get_psq(pc, to);
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
}

void Board::undo_move(const Move &m) {
    const MoveType mt = m.type();
    const Square from = m.from();
    const Square to = m.to();
    const Piece captured = states[curr_ply].captured;

    assert(m);
    assert(valid_piece(piece_at(to)));
    assert(!valid_piece(piece_at(from)));

    stm = ~stm;

    // decrement accum
    accum_list.pop();

    if(m.is_prom()) {
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
            Square cap_sqr = m.is_ep() ? Square(to ^ 8) : to;
            put_piece(captured, cap_sqr);
        }
    }

    curr_ply--;
}

void Board::make_nullmove() {
    curr_ply++;
    states[curr_ply] = StateInfo(states[curr_ply - 1]);
    StateInfo &info = states[curr_ply];

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

void Board::undo_nullmove() {
    curr_ply--;
    stm = ~stm;
}

void Board::update_accums() {
    assert(accum_list[0].is_initialized(WHITE));
    assert(accum_list[0].is_initialized(BLACK));

    NNUE::Accum &acc = get_accum();

    for(Color view : {WHITE, BLACK}) {
        if(acc.is_initialized(view))
            continue;

        const int accums_idx = accum_list.get_idx();
        assert(accums_idx > 0);

        // apply lazy update
        for(int i = accums_idx; i >= 0; i--) {
            if(accum_list[i].needs_refresh(view)) {
                accum_list.refresh(view);
                break;
            }

            if(accum_list[i].is_initialized(view)) {
                for(int j = i + 1; j <= accums_idx; j++)
                    accum_list[j].update(accum_list[j - 1], view);
                break;
            }
        }
    }

    assert(acc.is_initialized(WHITE));
    assert(acc.is_initialized(BLACK));
}

// private member

void Board::init_threats() {
    StateInfo &info = states[curr_ply];
    Color them = ~stm;
    U64 occ = get_occupancy() ^ square_bb(king_sq(stm)); // king must be excluded so we don't block the slider attacks

    U64 danger = 0;
    U64 threat = 0;

    // pawn attacks
    U64 pawns = get_piecebb(them, PAWN);
    threat = them == WHITE ? shift<NORTH_WEST>(pawns) | shift<NORTH_EAST>(pawns)
                           : shift<SOUTH_WEST>(pawns) | shift<SOUTH_EAST>(pawns);
    danger = threat;
    info.threats[PAWN] = threat;

    for(PieceType pt : {KNIGHT, BISHOP, ROOK, QUEEN, KING}) {
        U64 threat = 0;
        U64 pc_bb = get_piecebb(them, pt);
        while(pc_bb)
            threat |= get_attacks(pt, pop_lsb(pc_bb), occ);
        danger |= threat;
        info.threats[pt] = threat;
    }

    info.danger = danger;
    info.checkers = attackers_to(them, king_sq(stm), get_occupancy());
}

void Board::init_slider_blockers() {
    StateInfo &info = states[curr_ply];

    auto helper = [&](Color c) {
        info.blockers[c] = 0;
        info.pinners[~c] = 0;

        const Square ksq = king_sq(c);

        // potential enemy bishop, rook and queen attacks at our king
        U64 cands = (get_attacks(ROOK, ksq) & orth_sliders(~c)) | //
                    (get_attacks(BISHOP, ksq) & diag_sliders(~c));

        const U64 occ = get_occupancy() ^ cands;
        while(cands) {
            Square sq = pop_lsb(cands);
            U64 b = between_bb(ksq, sq) & occ;

            if(pop_count(b) == 1) {
                info.blockers[c] |= b;
                if(b & get_occupancy(c))
                    info.pinners[~c] |= square_bb(sq);
            }
        }
    };

    helper(WHITE);
    helper(BLACK);
}

} // namespace Chess
