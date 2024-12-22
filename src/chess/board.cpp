#include <cassert>
#include "board.h"

namespace Chess
{
    // represent castle rights correctly in fen notation
    bool castleNotationHelper(const std::ostringstream &fen_stream)
    {
        const std::string fen = fen_stream.str();
        const std::string rights = fen.substr(fen.find(' ') + 1);
        return rights.find_first_of("kqKQ") != std::string::npos;
    }

    // board class

    Board::Board(const std::string &fen) : piece_bb{}, board{}, stm(WHITE), curr_ply(0)
    {
        for (auto &i : board)
            i = NO_PIECE;
        history[0] = StateInfo();

        std::vector<std::string> fen_parts = split(fen, ' ');
        if (fen_parts.size() != 6)
        {
            std::cerr << "Invalid fen string" << std::endl;
            return;
        }

        stm = fen_parts[1] == "w" ? WHITE : BLACK;

        for (const char c : fen_parts[2])
        {
            switch (c)
            {
            case 'K':
                history[curr_ply].castle_rights.mask |= WHITE_OO_MASK;
                break;
            case 'Q':
                history[curr_ply].castle_rights.mask |= WHITE_OOO_MASK;
                break;
            case 'k':
                history[curr_ply].castle_rights.mask |= BLACK_OO_MASK;
                break;
            case 'q':
                history[curr_ply].castle_rights.mask |= BLACK_OOO_MASK;
                break;
            default:
                break;
            }
        }

        history[curr_ply].ep_sq = fen_parts[3] == "-" ? NO_SQUARE : squareFromString(fen_parts[3]);
        history[curr_ply].half_move_clock = std::stoi(fen_parts[4]);

        // place pieces to board
        int sqr = a8;
        for (const char c : fen_parts[0])
        {
            if (isdigit(c))
                sqr += c - '0';
            else if (c == '/')
                sqr -= 16;
            else
                putPiece(Piece(PIECE_STR.find(c)), Square(sqr++), true);
        }

        initThreats();
        initCheckersAndPinned();

        // initialize hash
        U64 hash = 0ULL;
        for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
        {
            U64 b = piece_bb[p];
            while (b)
                hash ^= Zobrist::getPsq(Piece(p), popLsb(b));
        }

        if (history[curr_ply].ep_sq != NO_SQUARE)
            hash ^= Zobrist::getEp(history[curr_ply].ep_sq);
        hash ^= Zobrist::getCastle(history[curr_ply].castle_rights.getHashIndex());
        hash ^= Zobrist::side;
        history[curr_ply].hash = hash;

        history[curr_ply].pawn_hash = Zobrist::getPawnZobrist(*this);

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
            << (history[curr_ply].castle_rights.kingSide(WHITE) ? "K" : "")
            << (history[curr_ply].castle_rights.queenSide(WHITE) ? "Q" : "")
            << (history[curr_ply].castle_rights.kingSide(BLACK) ? "k" : "")
            << (history[curr_ply].castle_rights.queenSide(BLACK) ? "q" : "")
            << (castleNotationHelper(fen) ? " " : "- ")
            << (history[curr_ply].ep_sq == NO_SQUARE ? "-" : SQSTR[history[curr_ply].ep_sq]);

        fen << " " << history[curr_ply].half_move_clock << " " << (curr_ply == 0 ? 1 : (curr_ply + 1) / 2);
        return fen.str();
    }

    void Board::refreshAccumulator()
    {
        accumulators.clear();
        NNUE::Accumulator &acc = accumulators.back();

        for (int j = 0; j < NNUE::HIDDEN_SIZE; j++)
        {
            acc.data[WHITE][j] = NNUE::nnue.fc1_biases[j];
            acc.data[BLACK][j] = NNUE::nnue.fc1_biases[j];
        }

        for (int i = WHITE_PAWN; i <= BLACK_KING; i++)
        {
            U64 b = piece_bb[i];
            while (b)
                NNUE::nnue.putPiece(acc, Piece(i), popLsb(b));
        }
    }

    U64 Board::occupancy(Color c) const
    {
        const int start_idx = (c == WHITE) ? 0 : 6;

        U64 occ = 0;
        for (int i = start_idx; i < start_idx + NUM_PIECE_TYPES; i++)
            occ |= piece_bb[i];

        return occ;
    }

    U64 Board::attackersTo(Color c, Square s, const U64 occ) const
    {
        U64 attacks = getPawnAttacks(~c, s) & getPieceBB(c, PAWN);
        attacks |= getAttacks(KNIGHT, s, occ) & getPieceBB(c, KNIGHT);
        attacks |= getAttacks(BISHOP, s, occ) & (getPieceBB(c, BISHOP) | getPieceBB(c, QUEEN));
        attacks |= getAttacks(ROOK, s, occ) & (getPieceBB(c, ROOK) | getPieceBB(c, QUEEN));
        attacks |= getAttacks(KING, s, occ) & getPieceBB(c, KING);
        return attacks;
    }

    U64 Board::keyAfter(Move m) const 
    {
        U64 new_hash = history[curr_ply].hash;

        if (!m)
            return new_hash ^ Zobrist::side;    

        Square from = m.from();
        Square to = m.to();

        Piece pc = pieceAt(from);
        Piece captured = pieceAt(to);

        if (captured != NO_PIECE)
            new_hash ^= Zobrist::getPsq(captured, to);

        new_hash ^= Zobrist::getPsq(pc, from) ^ Zobrist::getPsq(pc, to);
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

            assert(board[cap_sq] == makePiece(~stm, PAWN));
            assert(pieceAt(to) == NO_PIECE);

            U64 attackers = getAttacks(BISHOP, ksq, occ) & (getPieceBB(~stm, BISHOP) | getPieceBB(~stm, QUEEN));
            attackers |= getAttacks(ROOK, ksq, occ) & (getPieceBB(~stm, ROOK) | getPieceBB(~stm, QUEEN));
            // only legal if no discovered check occurs after ep capture
            return !attackers;
        }

        // only legal if not pinned or moving in the direction of the pin
        return !(history[curr_ply].pinned & SQUARE_BB[from]) || (LINE[from][ksq] & SQUARE_BB[to]);
    }

    bool Board::isPseudoLegal(const Move &m) const
    {
        const Square from = m.from();
        const Square to = m.to();
        const Square ksq = kingSq(stm);

        const Piece pc = pieceAt(from);
        const PieceType pt = typeOf(pc);

        const MoveType mt = m.type();

        const U64 us_bb = occupancy(stm);
        const U64 them_bb = occupancy(~stm);
        const U64 occ = us_bb | them_bb;

        const U64 checkers = history[curr_ply].checkers;
        const U64 danger = history[curr_ply].danger;

        const Direction up = stm == WHITE ? NORTH : SOUTH;

        // move must exist, piece must exist, piece must match the current stm
        if (!m || pc == NO_PIECE || colorOf(pc) != stm)
            return false;
        // if double check then only king can move
        if (sparsePopCount(checkers) > 1 && pt != KING)
            return false;
        // if capture move, then target square must be occupied by enemy piece
        if (mt != EN_PASSANT && isCap(m) && pieceAt(to) == NO_PIECE)
            return false;
        // if quiet move, then target square must be empty
        if (!isCap(m) && pieceAt(to) != NO_PIECE)
            return false;
        // we must not capture our own piece
        if (SQUARE_BB[to] & us_bb)
            return false;
        // king can't move to danger squares
        if (pt == KING && (SQUARE_BB[to] & danger))
            return false;

        if (mt == CASTLING)
        {
            // make sure we move a king
            if (pt != KING)
                return false;

            // can't castle when in check or when no castling rights are present
            if (checkers || !history[curr_ply].castle_rights.any(stm))
                return false;

            // short castling
            U64 not_free = (occ | danger) & ooBlockersMask(stm);
            if (!not_free && history[curr_ply].castle_rights.kingSide(stm) && to == relativeSquare(stm, g1))
                return true;

            // long castling
            not_free = (occ | (danger & ~SQUARE_BB[relativeSquare(stm, b1)])) & oooBlockersMask(stm);
            if (!not_free && history[curr_ply].castle_rights.queenSide(stm) && to == relativeSquare(stm, c1))
                return true;

            return false;
        }

        if (mt == EN_PASSANT)
        {
            Square ep_sq = history[curr_ply].ep_sq;

            // make sure we move a pawn
            if (pt != PAWN)
                return false;
            // to square must not be occupied
            if (pieceAt(to) != NO_PIECE)
                return false;
            // ep square must be the same
            if (ep_sq != to)
                return false;
            // pawn on ep square must be a enemy pawn
            if (pieceAt(ep_sq) != makePiece(~stm, PAWN))
                return false;
        }

        if (isProm(m))
        {
            // make sure we move a pawn
            if (pt != PAWN)
                return false;

            U64 targets = checkers ? SQUARES_BETWEEN[ksq][lsb(checkers)] | checkers : -1ULL;
            U64 rank = MASK_RANK[relativeRank(stm, RANK_8)];
            U64 attacks = (shift(up, SQUARE_BB[from]) & ~occ) | (getPawnAttacks(stm, from) & them_bb);
            // only pseudo legal, if target range is reachable
            return (rank & attacks & targets) & SQUARE_BB[to];
        }

        if (pt == PAWN && mt != EN_PASSANT)
        {
            // no promotion moves allowed here since we already handled them
            if (SQUARE_BB[to] & (MASK_RANK[RANK_1] | MASK_RANK[RANK_8]))
                return false;

            // is capture?
            bool capture = getPawnAttacks(stm, from) & them_bb & SQUARE_BB[to];
            // is single push?
            bool singe_push = (Square(from + up) == to) && pieceAt(to) == NO_PIECE;
            // is double push?
            bool double_push = relativeRank(stm, RANK_2) == rankOf(from) && (Square(from + 2 * up) == to) &&
                               pieceAt(to) == NO_PIECE && pieceAt(to - up) == NO_PIECE;

            // if none of the conditions above are met, then it's not pseudo legal
            if (!capture && !singe_push && !double_push)
                return false;
        }
        // if a non pawn piece's target range isn't reachable, then it's not pseudo legal
        else if (!(getAttacks(pt, from, occ) & SQUARE_BB[to]))
            return false;

        // check for blockers/captures of the checker
        if (checkers && pt != KING)
        {
            U64 target = SQUARES_BETWEEN[ksq][lsb(checkers)] | checkers;
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
        const Piece pc = pieceAt(from);
        const PieceType pt = typeOf(pc);
        const Piece captured = mt == EN_PASSANT ? makePiece(~stm, PAWN) : pieceAt(to);

        assert(typeOf(captured) != KING);
        assert(pc != NO_PIECE);

        curr_ply++;
        history[curr_ply] = StateInfo(history[curr_ply - 1]);
        history[curr_ply].half_move_clock++;

        // reset half move clock if pawn move or capture
        if (pt == PAWN || captured != NO_PIECE)
            history[curr_ply].half_move_clock = 0;

        history[curr_ply].hash ^= Zobrist::side;
        history[curr_ply].pawn_hash ^= Zobrist::side;
  
        if (update_nnue)
            accumulators.push();

        if (mt == CASTLING)
        {
            assert(pt == KING);
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

            // update hash of rook
            Piece rook = pieceAt(rook_from);

            assert(rook == makePiece(stm, ROOK));

            // update hash of rook
            history[curr_ply].hash ^= Zobrist::getPsq(rook, rook_from) ^ Zobrist::getPsq(rook, rook_to);
            // move rook
            movePiece(rook_from, rook_to, update_nnue);
        }

        if (captured != NO_PIECE)
        {
            Square cap_sq = mt == EN_PASSANT ? Square(to ^ 8) : to;
            // remove captured piece from hash
            history[curr_ply].hash ^= Zobrist::getPsq(captured, cap_sq);
            // remove captured piece from board
            removePiece(cap_sq, update_nnue);

            // update pawn hash if captured piece is a pawn
            if (typeOf(captured) == PAWN)
                history[curr_ply].pawn_hash ^= Zobrist::getPsq(captured, cap_sq);
        }

        // update hash of moving piece
        history[curr_ply].hash ^= Zobrist::getPsq(pc, from) ^ Zobrist::getPsq(pc, to);

        // reset ep square if exists
        if (history[curr_ply].ep_sq != NO_SQUARE)
        {
            history[curr_ply].hash ^= Zobrist::getEp(history[curr_ply].ep_sq);
            history[curr_ply].ep_sq = NO_SQUARE;
        }

        // update castling rights if king/rook moves or if one of the rooks get captured
        if (history[curr_ply].castle_rights.onCastleSquare(from) || history[curr_ply].castle_rights.onCastleSquare(to))
        {
            history[curr_ply].hash ^= Zobrist::getCastle(history[curr_ply].castle_rights.getHashIndex());
            history[curr_ply].castle_rights.mask &= ~(SQUARE_BB[from] | SQUARE_BB[to]);
            history[curr_ply].hash ^= Zobrist::getCastle(history[curr_ply].castle_rights.getHashIndex());
        }

        // move piece
        movePiece(from, to, update_nnue);

        if (pt == PAWN)
        {
            // update pawn hash
            history[curr_ply].pawn_hash ^= Zobrist::getPsq(pc, from) ^ Zobrist::getPsq(pc, to);

            // set ep square if double push can be captured by enemy pawn
            auto ep_sq = Square(to ^ 8);
            if ((from ^ to) == 16 && (getPawnAttacks(stm, ep_sq) & getPieceBB(~stm, PAWN)))
            {
                history[curr_ply].ep_sq = ep_sq;
                history[curr_ply].hash ^= Zobrist::getEp(ep_sq);
            }
            else if (isProm(m))
            {
                PieceType prom_type = typeOfPromotion(mt);
                Piece prom_pc = makePiece(stm, prom_type);

                assert(prom_type != PAWN);
                assert(prom_pc != NO_PIECE);

                // update hash
                history[curr_ply].hash ^= Zobrist::getPsq(pc, to) ^ Zobrist::getPsq(prom_pc, to);

                // add promoted piece and remove pawn
                removePiece(to, update_nnue);
                putPiece(prom_pc, to, update_nnue);

                // update pawn hash
                history[curr_ply].pawn_hash ^= Zobrist::getPsq(pc, to);
            }
        }

        stm = ~stm;

        // set captured piece
        history[curr_ply].captured = captured;

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
            Square rook_from, rook_to;

            if (to == relativeSquare(stm, g1)) // kingside
            {
                rook_from = relativeSquare(stm, f1);
                rook_to = relativeSquare(stm, h1);
            }
            else // queenside
            {
                rook_from = relativeSquare(stm, d1);
                rook_to = relativeSquare(stm, a1);
            }

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

        if (history[curr_ply].ep_sq != NO_SQUARE)
        {
            history[curr_ply].hash ^= Zobrist::getEp(history[curr_ply].ep_sq);
            history[curr_ply].ep_sq = NO_SQUARE;
        }

        history[curr_ply].hash ^= Zobrist::side;
        history[curr_ply].pawn_hash ^= Zobrist::side;

        history[curr_ply].half_move_clock++;

        stm = ~stm;

        initThreats();
        initCheckersAndPinned();
    }

    void Board::unmakeNullMove()
    {
        curr_ply--;
        stm = ~stm;
    }

    bool Board::isRepetition(bool is_pv) const
    {
        int count = 0;
        int limit = curr_ply - history[curr_ply].half_move_clock - 1;

        for (int i = curr_ply - 2; i >= 0 && i >= limit; i -= 2)
            if (history[i].hash == history[curr_ply].hash)
                if (++count == 1 + is_pv)
                    return true;

        return false;
    }

    bool Board::isInsufficientMaterial() const
    {
        U64 pawns = piece_bb[WHITE_PAWN] | piece_bb[BLACK_PAWN];
        U64 queens = piece_bb[WHITE_QUEEN] | piece_bb[BLACK_QUEEN];
        U64 rooks = piece_bb[WHITE_ROOK] | piece_bb[BLACK_ROOK];

        if (pawns || queens || rooks)
            return false;

        int nw_knights = popCount(piece_bb[WHITE_KNIGHT]);
        int nb_knights = popCount(piece_bb[BLACK_KNIGHT]);
        int nw_bishops = popCount(piece_bb[WHITE_BISHOP]);
        int nb_bishops = popCount(piece_bb[BLACK_BISHOP]);

        // Kvk
        if (!nw_knights && !nw_bishops && !nb_knights && !nb_bishops)
            return true;
        // KNvk and KNNvk
        if (!nb_knights && !nb_bishops && !nw_bishops && nw_knights <= 2)
            return true;
        // Kvkn and Kvknn
        if (!nw_knights && !nw_bishops && !nb_bishops && nb_knights <= 2)
            return true;
        // KNvkn
        if (!nw_bishops && !nb_bishops && nw_knights == 1 && nb_knights == 1)
            return true;
        // KBvk
        if (!nw_knights && !nb_knights && !nb_bishops && nw_bishops == 1)
            return true;
        // Kvkb
        if (!nw_knights && !nb_knights && !nw_bishops && nb_bishops == 1)
            return true;
        // KBvkn
        if (!nw_knights && !nb_bishops && nw_bishops == 1 && nb_knights == 1)
            return true;
        // KNvkb
        if (!nw_bishops && !nb_knights && nw_knights == 1 && nb_bishops == 1)
            return true;
        // KBvkb
        if (!nw_knights && !nb_knights && nw_bishops == 1 && nb_bishops == 1)
            return true;

        return false;
    }

    // doesn't include stalemate or threefold
    bool Board::isDraw() const
    {
        return history[curr_ply].half_move_clock > 99 || isInsufficientMaterial();
    }

    bool Board::see(Move &move, int threshold) const
    {
        if (isProm(move) || move.type() == EN_PASSANT || move.type() == CASTLING)
            return true;

        const Square from = move.from();
        const Square to = move.to();
        const PieceType attacker = typeOf(pieceAt(from));
        const PieceType victim = typeOf(pieceAt(to));

        assert(attacker != NO_PIECE_TYPE);

        int swap = PIECE_VALUES[victim] - threshold;
        if (swap < 0)
            return false;

        swap = PIECE_VALUES[attacker] - swap;

        if (swap <= 0)
            return true;

        U64 occ = occupancy();
        occ = occ ^ SQUARE_BB[from] ^ SQUARE_BB[to];
        U64 attackers = attackersTo(WHITE, to, occ) | attackersTo(BLACK, to, occ);

        U64 diag = diagSliders(WHITE) | diagSliders(BLACK);
        U64 orth = orthSliders(WHITE) | orthSliders(BLACK);

        bool result = true;

        Color curr_stm = getTurn();

        U64 my_attacker, least_attacker;
        while (true)
        {
            curr_stm = ~curr_stm;
            attackers &= occ;

            if (!(my_attacker = (attackers & occupancy(curr_stm))))
                break;
            result = !result;

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

        return result;
    }

    // private member

    void Board::initThreats()
    {
        Color them = ~stm;
        U64 occ = occupancy() ^ SQUARE_BB[kingSq(stm)]; // king must be excluded so we don't block the slider attacks

        U64 danger = 0;
        U64 threat = 0;

        // pawn attacks
        U64 pawns = getPieceBB(them, PAWN);
        threat = them == WHITE ? shift(NORTH_WEST, pawns) | shift(NORTH_EAST, pawns) : shift(SOUTH_WEST, pawns) | shift(SOUTH_EAST, pawns);
        danger = threat;

        history[curr_ply].threats[PAWN] = threat;

        // knight attacks
        threat = 0;
        U64 knights = getPieceBB(them, KNIGHT);
        while (knights)
            threat |= getAttacks(KNIGHT, popLsb(knights), occ);
        danger |= threat;

        history[curr_ply].threats[KNIGHT] = threat;

        // bishop attacks
        threat = 0;
        U64 bishops = getPieceBB(them, BISHOP);
        while (bishops)
            threat |= getAttacks(BISHOP, popLsb(bishops), occ);
        danger |= threat;

        history[curr_ply].threats[BISHOP] = threat;

        // rook attacks
        threat = 0;
        U64 rooks = getPieceBB(them, ROOK);
        while (rooks)
            threat |= getAttacks(ROOK, popLsb(rooks), occ);
        danger |= threat;

        history[curr_ply].threats[ROOK] = threat;

        // queen attacks
        threat = 0;
        U64 queens = getPieceBB(them, QUEEN);
        while (queens)
            threat |= getAttacks(QUEEN, popLsb(queens), occ);
        danger |= threat;

        history[curr_ply].threats[QUEEN] = threat;

        // king attacks
        threat = getAttacks(KING, kingSq(them), occ);
        danger |= threat;

        history[curr_ply].threats[KING] = threat;
        history[curr_ply].danger = danger;
    }

    void Board::initCheckersAndPinned()
    {
        const Square ksq = kingSq(stm);
        const Color them = ~stm;
        const U64 their_occ = occupancy(them);
        const U64 our_occ = occupancy(stm);

        // enemy pawns attacks at our king
        history[curr_ply].checkers = getPawnAttacks(stm, ksq) & getPieceBB(them, PAWN);
        // enemy knights attacks at our king
        history[curr_ply].checkers |= getAttacks(KNIGHT, ksq, our_occ | their_occ) & getPieceBB(them, KNIGHT);

        // potential enemy bishop, rook and queen attacks at our king
        U64 candidates = (getAttacks(ROOK, ksq, their_occ) & orthSliders(them)) | (getAttacks(BISHOP, ksq, their_occ) & diagSliders(them));

        history[curr_ply].pinned = 0;
        while (candidates)
        {
            Square s = popLsb(candidates);
            U64 blockers = SQUARES_BETWEEN[ksq][s] & our_occ;
            if (!blockers)
                // if no of out pieces is between the enemy slider, then add that piece as checker
                history[curr_ply].checkers ^= SQUARE_BB[s];
            else if (sparsePopCount(blockers) == 1)
                // if we have only one blocker, add that piece as pinned
                history[curr_ply].pinned ^= blockers;
        }
    }

} // namespace Chess
