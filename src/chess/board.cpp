#include <iostream>
#include <cassert>
#include "board.h"

namespace Chess
{
    // helper

    // represent castle rights correctly in fen notation
    bool castleNotationHelper(const std::ostringstream &fen_stream)
    {
        const std::string fen = fen_stream.str();
        const std::string rights = fen.substr(fen.find(' ') + 1);
        return rights.find_first_of("kqKQ") != std::string::npos;
    }

    std::pair<Square, Square> getCastleRookSquares(Color stm, Square to)
    {
        Square rook_from, rook_to;

        if (to == relativeSquare(stm, g1)) // kingside
        {
            rook_from = relativeSquare(stm, h1);
            rook_to = relativeSquare(stm, f1);
        }
        else // queenside
        {
            rook_from = relativeSquare(stm, a1);
            rook_to = relativeSquare(stm, d1);
        }

        return {rook_from, rook_to};
    }

    // board

    Board::Board(const std::string &fen, bool update_nnue) : piece_bb{}, board{}, stm(WHITE), curr_ply(0)
    {
        for (auto &i : board)
            i = NO_PIECE;
        history[0] = StateInfo();
        StateInfo &info = history[0];

        std::vector<std::string> fen_parts = split(fen, ' ');
        if (fen_parts.size() != 6)
        {
            std::cerr << "Invalid fen string" << std::endl;
            return;
        }

        stm = fen_parts[1] == "w" ? WHITE : BLACK;

        for (const char c : fen_parts[2])
        {
            // clang-format off
            switch (c)
            {
            case 'K': info.castle_rights.mask |= OO_MASK[WHITE]; break;
            case 'Q': info.castle_rights.mask |= OOO_MASK[WHITE]; break;
            case 'k': info.castle_rights.mask |= OO_MASK[BLACK]; break;
            case 'q': info.castle_rights.mask |= OOO_MASK[BLACK]; break;
            default: break;
            }
            // clang-format on
        }

        info.ep_sq = fen_parts[3] == "-" ? NO_SQUARE : squareFromString(fen_parts[3]);
        info.half_move_clock = std::stoi(fen_parts[4]);

        // place pieces on the board
        int sqr = a8;
        for (const char c : fen_parts[0])
        {
            if (isdigit(c))
                sqr += c - '0';
            else if (c == '/')
                sqr -= 16;
            else
                putPiece(Piece(PIECE_STR.find(c)), Square(sqr++), false);
        }

        initThreats();
        initCheckersAndPinned();

        // initialize hash
        info.pawn_hash = Zobrist::getPawnZobrist(*this);
        info.non_pawn_hash[WHITE] = Zobrist::getNonPawnZobrist(*this, WHITE);
        info.non_pawn_hash[BLACK] = Zobrist::getNonPawnZobrist(*this, BLACK);

        info.hash = info.pawn_hash ^ info.non_pawn_hash[WHITE] ^ info.non_pawn_hash[BLACK];
        if (info.ep_sq != NO_SQUARE)
            info.hash ^= Zobrist::getEp(info.ep_sq);
        info.hash ^= Zobrist::getCastle(info.castle_rights.getHashIndex());
        info.hash ^= Zobrist::side;

        if (update_nnue)
            refreshAccumulator();
    }

    Board &Board::operator=(const Board &other)
    {
        if (this != &other)
        {
            curr_ply = other.curr_ply;
            stm = other.stm;

            std::copy(std::begin(other.piece_bb), std::end(other.piece_bb), std::begin(piece_bb));
            std::copy(std::begin(other.board), std::end(other.board), std::begin(board));
            std::copy(std::begin(other.history), std::end(other.history), std::begin(history));
            accumulators = other.accumulators;
        }

        return *this;
    }

    Board::Board(const Board &other)
    {
        curr_ply = other.curr_ply;
        stm = other.stm;

        std::copy(std::begin(other.piece_bb), std::end(other.piece_bb), std::begin(piece_bb));
        std::copy(std::begin(other.board), std::end(other.board), std::begin(board));
        std::copy(std::begin(other.history), std::end(other.history), std::begin(history));
        accumulators = other.accumulators;
    }

    void Board::print() const
    {
        std::cout << "\n +---+---+---+---+---+---+---+---+\n";

        for (int r = RANK_8; r >= RANK_1; --r)
        {
            for (int f = FILE_A; f <= FILE_H; ++f)
            {
                int s;
                if (stm == WHITE)
                    s = r * 8 + f;
                else
                    s = (7 - r) * 8 + (7 - f);
                std::cout << " | " << PIECE_STR[board[s]];
            }
            std::cout << " | " << (1 + r) << "\n +---+---+---+---+---+---+---+---+\n";
        }

        std::cout << "   a   b   c   d   e   f   g   h\n";
        std::cout << "\nFen: " << getFen() << std::endl;
        std::cout << "Hash key: " << std::hex << history[curr_ply].hash << std::dec << "\n\n";
    }

    std::string Board::getFen() const
    {
        const StateInfo &info = history[curr_ply];
        std::ostringstream fen;

        for (int i = 56; i >= 0; i -= 8)
        {
            int empty = 0;

            for (int j = 0; j < 8; j++)
            {
                Piece p = board[i + j];
                if (p == NO_PIECE)
                    empty++;
                else
                {
                    fen << (empty == 0 ? "" : std::to_string(empty)) << PIECE_STR[p];
                    empty = 0;
                }
            }

            if (empty != 0)
                fen << empty;
            if (i > 0)
                fen << '/';
        }

        fen << (stm == WHITE ? " w " : " b ")
            << (info.castle_rights.kingSide(WHITE) ? "K" : "")
            << (info.castle_rights.queenSide(WHITE) ? "Q" : "")
            << (info.castle_rights.kingSide(BLACK) ? "k" : "")
            << (info.castle_rights.queenSide(BLACK) ? "q" : "")
            << (castleNotationHelper(fen) ? " " : "- ")
            << (info.ep_sq == NO_SQUARE ? "-" : SQSTR[info.ep_sq]);

        fen << " " << info.half_move_clock << " " << (curr_ply == 0 ? 1 : (curr_ply + 1) / 2);
        return fen.str();
    }

    void Board::refreshAccumulator()
    {
        accumulators.clear();
        accumulator_table->reset();
        accumulator_table->refresh(WHITE, *this);
        accumulator_table->refresh(BLACK, *this);
    }

    U64 Board::attackersTo(Color c, Square s, const U64 occ) const
    {
        U64 attacks = getPawnAttacks(~c, s) & getPieceBB(c, PAWN);
        attacks |= getKnightAttacks(s) & getPieceBB(c, KNIGHT);
        attacks |= getBishopAttacks(s, occ) & diagSliders(c);
        attacks |= getRookAttacks(s, occ) & orthSliders(c);
        attacks |= getKingAttacks(s) & getPieceBB(c, KING);
        return attacks;
    }

    U64 Board::keyAfter(Move m) const
    {
        U64 new_hash = history[curr_ply].hash;

        if (!m)
            return new_hash ^ Zobrist::side;

        Square from = m.from();
        Square to = m.to();

        Piece p = pieceAt(from);
        Piece captured = pieceAt(to);

        if (captured != NO_PIECE)
            new_hash ^= Zobrist::getPsq(captured, to);

        new_hash ^= Zobrist::getPsq(p, from) ^ Zobrist::getPsq(p, to);
        new_hash ^= Zobrist::side;

        return new_hash;
    }

    bool Board::nonPawnMat(Color c) const
    {
        return getPieceBB(c, KNIGHT) | getPieceBB(c, BISHOP) | getPieceBB(c, ROOK) | getPieceBB(c, QUEEN);
    }

    // only checks ep and pinned piece moves
    bool Board::isLegal(const Move &m) const
    {
        const Square from = m.from();
        const Square to = m.to();
        const Square ksq = kingSq(stm);

        assert(pieceAt(from) != NO_PIECE);
        assert(pieceAt(ksq) == makePiece(stm, KING));

        if (m.type() == EN_PASSANT)
        {
            Square cap_sq = Square(to ^ 8);
            U64 occ = (occupancy() ^ SQUARE_BB[from] ^ SQUARE_BB[cap_sq]) | SQUARE_BB[to];

            assert(pieceAt(cap_sq) == makePiece(~stm, PAWN));
            assert(pieceAt(to) == NO_PIECE);

            U64 attackers = getBishopAttacks(ksq, occ) & (getPieceBB(~stm, BISHOP) | getPieceBB(~stm, QUEEN));
            attackers |= getRookAttacks(ksq, occ) & (getPieceBB(~stm, ROOK) | getPieceBB(~stm, QUEEN));
            // only legal if no discovered check occurs after ep capture
            return !attackers;
        }

        // only legal if not pinned or moving in the direction of the pin
        return !(history[curr_ply].pinned & SQUARE_BB[from]) || (LINE[from][ksq] & SQUARE_BB[to]);
    }

    bool Board::isPseudoLegal(const Move &m) const
    {
        const StateInfo &info = history[curr_ply];

        const Square from = m.from();
        const Square to = m.to();
        const Square ksq = kingSq(stm);

        const Piece from_p = pieceAt(from);
        const Piece to_p = pieceAt(to);
        const PieceType pt = typeOf(from_p);

        const MoveType mt = m.type();

        const U64 us_bb = occupancy(stm);
        const U64 them_bb = occupancy(~stm);
        const U64 occ = us_bb | them_bb;

        const Direction up = stm == WHITE ? NORTH : SOUTH;

        // move must exist, piece must exist, piece must match the current stm
        if (!m || from_p == NO_PIECE || colorOf(from_p) != stm)
            return false;
        // if double check, then only king can move
        if (popCount(info.checkers) > 1 && pt != KING)
            return false;
        // if capture move, then target square must be occupied by enemy piece
        if (mt != EN_PASSANT && isCap(m) && to_p == NO_PIECE)
            return false;
        // if quiet move, then target square must be empty
        if (!isCap(m) && to_p != NO_PIECE)
            return false;
        // we must not capture our own piece
        if (SQUARE_BB[to] & us_bb)
            return false;
        // king can't move to danger squares
        if (pt == KING && (SQUARE_BB[to] & info.danger))
            return false;

        if (mt == CASTLING)
        {
            // make sure we move a king
            if (pt != KING)
                return false;
            // can't castle when in check or when no castling rights are present
            if (info.checkers || !info.castle_rights.any(stm))
                return false;
            // short castling
            U64 not_free = (occ | info.danger) & OO_BLOCKERS_MASK[stm];
            if (!not_free && info.castle_rights.kingSide(stm) && to == relativeSquare(stm, g1))
                return true;
            // long castling
            not_free = (occ | (info.danger & ~SQUARE_BB[relativeSquare(stm, b1)])) & OOO_BLOCKERS_MASK[stm];
            if (!not_free && info.castle_rights.queenSide(stm) && to == relativeSquare(stm, c1))
                return true;
            // if short/long castling condition is not met, then it's not pseudo legal
            return false;
        }

        if (mt == EN_PASSANT)
        {
            // make sure we move a pawn
            if (pt != PAWN)
                return false;
            // to-square must not be occupied
            if (to_p != NO_PIECE)
                return false;
            // ep square must be same
            if (info.ep_sq != to)
                return false;
            // pawn on ep square must be a enemy pawn
            if (pieceAt(Square(info.ep_sq ^ 8)) != makePiece(~stm, PAWN))
                return false;
        }

        if (isProm(m))
        {
            // make sure we move a pawn
            if (pt != PAWN)
                return false;

            U64 targets = info.checkers ? SQUARES_BETWEEN[ksq][lsb(info.checkers)] | info.checkers : -1ULL;
            U64 rank = MASK_RANK[relativeRank(stm, RANK_8)];
            U64 attacks = (shift(up, SQUARE_BB[from]) & ~occ) | (getPawnAttacks(stm, from) & them_bb);
            // only pseudo legal, if target range is reachable
            return (rank & attacks & targets) & SQUARE_BB[to];
        }

        if (pt == PAWN)
        {
            // no promotion moves allowed here since we already handled them
            if (SQUARE_BB[to] & (MASK_RANK[RANK_1] | MASK_RANK[RANK_8]))
                return false;

            if (mt != EN_PASSANT)
            {
                // is capture?
                bool capture = getPawnAttacks(stm, from) & them_bb & SQUARE_BB[to];
                // is single push?
                bool singe_push = (Square(from + up) == to) && to_p == NO_PIECE;
                // is double push?
                bool double_push = relativeRank(stm, RANK_2) == rankOf(from) && (Square(from + 2 * up) == to) && (to_p + pieceAt(to - up)) == 2 * NO_PIECE;

                // if none of the conditions above are met, then it's not pseudo legal
                if (!capture && !singe_push && !double_push)
                    return false;
            }
        }
        // if a non pawn piece's target range isn't reachable, then it's not pseudo legal
        else if (!(getAttacks(pt, from, occ) & SQUARE_BB[to]))
            return false;

        // check for blockers/captures of the checker
        if (info.checkers && pt != KING)
        {
            U64 target = SQUARES_BETWEEN[ksq][lsb(info.checkers)] | info.checkers;
            Square cap_sq = mt == EN_PASSANT ? Square(to ^ 8) : to;

            // if move can't capture/block the checker, then it's not pseudo legal
            if (!(target & SQUARE_BB[cap_sq]))
                return false;
        }

        return true;
    }

    void Board::makeMove(const Move &m, bool update_nnue)
    {
        const MoveType mt = m.type();
        const Square from = m.from();
        const Square to = m.to();
        const Piece p = pieceAt(from);
        const PieceType pt = typeOf(p);
        const Piece captured = mt == EN_PASSANT ? makePiece(~stm, PAWN) : pieceAt(to);

        assert(typeOf(captured) != KING);
        assert(p != NO_PIECE);

        curr_ply++;
        history[curr_ply] = StateInfo(history[curr_ply - 1]);
        StateInfo &info = history[curr_ply];

        info.half_move_clock++;
        info.plies_from_null++;

        // reset half move clock if pawn move or capture
        if (pt == PAWN || captured != NO_PIECE)
            info.half_move_clock = 0;

        // update hash
        info.hash ^= Zobrist::side;

        if (update_nnue)
            accumulators.push();

        if (mt == CASTLING)
        {
            assert(pt == KING);

            auto [rook_from, rook_to] = getCastleRookSquares(stm, to);
            Piece rook = pieceAt(rook_from);

            assert(rook == makePiece(stm, ROOK));

            // update hash of moving rook
            info.hash ^= Zobrist::getPsq(rook, rook_from) ^ Zobrist::getPsq(rook, rook_to);
            info.non_pawn_hash[stm] ^= Zobrist::getPsq(rook, rook_from) ^ Zobrist::getPsq(rook, rook_to);

            // move rook
            movePiece(rook_from, rook_to, update_nnue);
        }

        if (captured != NO_PIECE)
        {
            Square cap_sq = mt == EN_PASSANT ? Square(to ^ 8) : to;
            // remove captured piece from hash
            info.hash ^= Zobrist::getPsq(captured, cap_sq);
            // remove captured piece from board
            removePiece(cap_sq, update_nnue);

            // update hash
            if (typeOf(captured) == PAWN)
                info.pawn_hash ^= Zobrist::getPsq(captured, cap_sq);
            else
                info.non_pawn_hash[~stm] ^= Zobrist::getPsq(captured, cap_sq);
        }

        // update hash of moving piece
        info.hash ^= Zobrist::getPsq(p, from) ^ Zobrist::getPsq(p, to);
        if (pt == PAWN)
            info.pawn_hash ^= Zobrist::getPsq(p, from) ^ Zobrist::getPsq(p, to);
        else
            info.non_pawn_hash[stm] ^= Zobrist::getPsq(p, from) ^ Zobrist::getPsq(p, to);

        // reset ep square if it exists
        if (info.ep_sq != NO_SQUARE)
        {
            // remove ep square from hash
            info.hash ^= Zobrist::getEp(info.ep_sq);
            info.ep_sq = NO_SQUARE;
        }

        // update castling rights if king/rook moves or if one of the rooks get captured
        if (info.castle_rights.onCastleSquare(from) || info.castle_rights.onCastleSquare(to))
        {
            // remove old castling rights from hash
            info.hash ^= Zobrist::getCastle(info.castle_rights.getHashIndex());
            info.castle_rights.mask &= ~(SQUARE_BB[from] | SQUARE_BB[to]);
            // add new castling rights to hash
            info.hash ^= Zobrist::getCastle(info.castle_rights.getHashIndex());
        }

        // move piece
        movePiece(from, to, update_nnue);

        if (pt == PAWN)
        {
            // set ep square if double push can be captured by enemy pawn
            auto ep_sq = Square(to ^ 8);
            if ((from ^ to) == 16 && (getPawnAttacks(stm, ep_sq) & getPieceBB(~stm, PAWN)))
            {
                info.ep_sq = ep_sq;
                // add ep square to hash
                info.hash ^= Zobrist::getEp(ep_sq);
            }
            else if (isProm(m))
            {
                PieceType prom_type = typeOfPromotion(mt);
                Piece prom_p = makePiece(stm, prom_type);

                assert(prom_type != PAWN);
                assert(prom_p != NO_PIECE);

                // update hash of promoting piece
                info.hash ^= Zobrist::getPsq(p, to) ^ Zobrist::getPsq(prom_p, to);
                info.pawn_hash ^= Zobrist::getPsq(p, to);

                // add promoted piece and remove pawn
                removePiece(to, update_nnue);
                putPiece(prom_p, to, update_nnue);
            }
        }

        stm = ~stm;
        info.captured = captured;

        initThreats();
        initCheckersAndPinned();
    }

    void Board::unmakeMove(const Move &m)
    {
        stm = ~stm;

        const MoveType mt = m.type();
        const Square from = m.from();
        const Square to = m.to();
        const Piece captured = history[curr_ply].captured;

        assert(pieceAt(to) != NO_PIECE);
        assert(pieceAt(from) == NO_PIECE || mt == CASTLING);

        if (accumulators.size())
            accumulators.pop();

        if (isProm(m))
        {
            removePiece(to, false);
            putPiece(makePiece(stm, PAWN), to, false);
        }

        if (mt == CASTLING)
        {
            auto [rook_to, rook_from] = getCastleRookSquares(stm, to);

            movePiece(to, from, false); // move king
            movePiece(rook_from, rook_to, false);
        }
        else
        {
            movePiece(to, from, false);

            if (captured != NO_PIECE)
            {
                Square cap_sqr = mt == EN_PASSANT ? Square(to ^ 8) : to;
                putPiece(captured, cap_sqr, false);
            }
        }

        curr_ply--;
    }

    void Board::makeNullMove()
    {
        curr_ply++;
        history[curr_ply] = StateInfo(history[curr_ply - 1]);
        StateInfo &info = history[curr_ply];

        info.half_move_clock++;
        info.plies_from_null = 0;

        if (info.ep_sq != NO_SQUARE)
        {
            // remove ep square from hash
            info.hash ^= Zobrist::getEp(info.ep_sq);
            info.ep_sq = NO_SQUARE;
        }

        // update hash
        info.hash ^= Zobrist::side;

        stm = ~stm;

        initThreats();
        initCheckersAndPinned();
    }

    void Board::unmakeNullMove()
    {
        curr_ply--;
        stm = ~stm;
    }

    bool Board::isRepetition(int ply) const
    {
        const StateInfo &info = history[curr_ply];
        int rep = 0;
        int distance = std::min(info.plies_from_null, info.half_move_clock);

        for (int i = curr_ply - 4; i >= 0 && i >= curr_ply - distance; i -= 2)
        {
            if (history[i].hash == info.hash)
            {
                if (i > curr_ply - ply)
                    return true;
                rep++;
                if (rep == 2)
                    return true;
            }
        }

        return false;
    }

    // doesn't include stalemate, threefold and Insufficient material
    bool Board::isDraw(int ply) const
    {
        return history[curr_ply].half_move_clock > 99 || isRepetition(ply);
    }

    bool Board::see(Move &m, int threshold) const
    {
        if (isProm(m) || m.type() == EN_PASSANT || m.type() == CASTLING)
            return true;

        const Square from = m.from();
        const Square to = m.to();
        const PieceType attacker = typeOf(pieceAt(from));
        const PieceType victim = typeOf(pieceAt(to));

        assert(attacker != NO_PIECE_TYPE);

        int swap = PIECE_VALUES[victim] - threshold;
        if (swap < 0)
            return false;

        swap = PIECE_VALUES[attacker] - swap;

        if (swap <= 0)
            return true;

        U64 occ = occupancy() ^ SQUARE_BB[from] ^ SQUARE_BB[to];
        U64 attackers = attackersTo(WHITE, to, occ) | attackersTo(BLACK, to, occ);

        const U64 diag = diagSliders(WHITE) | diagSliders(BLACK);
        const U64 orth = orthSliders(WHITE) | orthSliders(BLACK);

        int result = 1;

        Color curr_stm = getTurn();

        U64 my_attacker, least_attacker;
        while (true)
        {
            curr_stm = ~curr_stm;
            attackers &= occ;

            if (!(my_attacker = (attackers & occupancy(curr_stm))))
                break;
            result ^= 1;

            if ((least_attacker = my_attacker & getPieceBB(curr_stm, PAWN)))
            {
                if ((swap = PIECE_VALUES[PAWN] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= getBishopAttacks(to, occ) & diag;
            }
            else if ((least_attacker = my_attacker & getPieceBB(curr_stm, KNIGHT)))
            {
                if ((swap = PIECE_VALUES[KNIGHT] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
            }
            else if ((least_attacker = my_attacker & getPieceBB(curr_stm, BISHOP)))
            {
                if ((swap = PIECE_VALUES[BISHOP] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= getBishopAttacks(to, occ) & diag;
            }
            else if ((least_attacker = my_attacker & getPieceBB(curr_stm, ROOK)))
            {
                if ((swap = PIECE_VALUES[ROOK] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= getRookAttacks(to, occ) & orth;
            }
            else if ((least_attacker = my_attacker & getPieceBB(curr_stm, QUEEN)))
            {
                if ((swap = PIECE_VALUES[QUEEN] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= (getBishopAttacks(to, occ) & diag) | (getRookAttacks(to, occ) & orth);
            }
            else
                return (attackers & ~occupancy(curr_stm)) ? result ^ 1 : result;
        }

        return bool(result);
    }

    bool Board::hasUpcomingRepetition(int ply)
    {
        const U64 occ = occupancy();

        const StateInfo &info = history[curr_ply];
        StateInfo *prev = &history[curr_ply - 1];

        int distance = std::min(info.half_move_clock, info.plies_from_null);
        for (int i = 3; i <= distance; i += 2)
        {
            prev -= 2;
            U64 move_key = info.hash ^ prev->hash;

            int hash = Cuckoo::cuckooH1(move_key);
            if (Cuckoo::keys[hash] != move_key)
                hash = Cuckoo::cuckooH2(move_key);

            if (Cuckoo::keys[hash] != move_key)
                continue;

            Move move = Cuckoo::cuckoo_moves[hash];
            Square from = move.from();
            Square to = move.to();

            U64 between = SQUARES_BETWEEN[from][to] ^ SQUARE_BB[to];
            if (between & occ)
                continue;

            if (ply > i)
                return true;

            Piece p = pieceAt(from) != NO_PIECE ? pieceAt(from) : pieceAt(to);
            if (colorOf(p) != stm)
                continue;

            StateInfo *prev2 = prev - 2;
            for (int j = i + 4; j <= distance; j += 2)
            {
                prev2 -= 2;
                if (prev2->hash == prev->hash)
                    return true;
            }
        }

        return false;
    }

    // private member

    void Board::initThreats()
    {
        StateInfo &info = history[curr_ply];
        Color them = ~stm;
        U64 occ = occupancy() ^ SQUARE_BB[kingSq(stm)]; // king must be excluded so we don't block the slider attacks

        U64 danger = 0;
        U64 threat = 0;

        // pawn attacks
        U64 pawns = getPieceBB(them, PAWN);
        threat = them == WHITE ? shift(NORTH_WEST, pawns) | shift(NORTH_EAST, pawns) : shift(SOUTH_WEST, pawns) | shift(SOUTH_EAST, pawns);
        danger = threat;
        info.threats[PAWN] = threat;

        // knight attacks
        threat = 0;
        U64 knights = getPieceBB(them, KNIGHT);
        while (knights)
            threat |= getKnightAttacks(popLsb(knights));
        danger |= threat;
        info.threats[KNIGHT] = threat;

        // bishop attacks
        threat = 0;
        U64 bishops = getPieceBB(them, BISHOP);
        while (bishops)
            threat |= getBishopAttacks(popLsb(bishops), occ);
        danger |= threat;
        info.threats[BISHOP] = threat;

        // rook attacks
        threat = 0;
        U64 rooks = getPieceBB(them, ROOK);
        while (rooks)
            threat |= getRookAttacks(popLsb(rooks), occ);
        danger |= threat;
        info.threats[ROOK] = threat;

        // queen attacks
        threat = 0;
        U64 queens = getPieceBB(them, QUEEN);
        while (queens)
            threat |= getAttacks(QUEEN, popLsb(queens), occ);
        danger |= threat;
        info.threats[QUEEN] = threat;

        // king attacks
        threat = getKingAttacks(kingSq(them));
        danger |= threat;
        info.threats[KING] = threat;

        info.danger = danger;
    }

    void Board::initCheckersAndPinned()
    {
        StateInfo &info = history[curr_ply];

        const Square ksq = kingSq(stm);
        const Color them = ~stm;
        const U64 their_occ = occupancy(them);
        const U64 our_occ = occupancy(stm);

        // enemy pawns attacks at our king
        info.checkers = getPawnAttacks(stm, ksq) & getPieceBB(them, PAWN);
        // enemy knights attacks at our king
        info.checkers |= getKnightAttacks(ksq) & getPieceBB(them, KNIGHT);

        // potential enemy bishop, rook and queen attacks at our king
        U64 candidates = (getRookAttacks(ksq, their_occ) & orthSliders(them)) | (getBishopAttacks(ksq, their_occ) & diagSliders(them));

        info.pinned = 0;
        while (candidates)
        {
            Square s = popLsb(candidates);
            U64 blockers = SQUARES_BETWEEN[ksq][s] & our_occ;
            if (!blockers)
                // if no piece is between the enemy slider, then add that piece as checker
                info.checkers ^= SQUARE_BB[s];
            else if (popCount(blockers) == 1)
                // if we have only one blocker, add that piece as pinned
                info.pinned ^= blockers;
        }
    }

} // namespace Chess
