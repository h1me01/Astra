#include <cassert>
#include "board.h"
#include "../search/search.h"

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
            case 'K': history[curr_ply].castle_rights.mask |= WHITE_OO_MASK; break;
            case 'Q': history[curr_ply].castle_rights.mask |= WHITE_OOO_MASK; break;
            case 'k': history[curr_ply].castle_rights.mask |= BLACK_OO_MASK; break;
            case 'q': history[curr_ply].castle_rights.mask |= BLACK_OOO_MASK; break;
            default: break;
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

        // initialize hash
        hash = 0ULL;
        for (int p = WHITE_PAWN; p <= BLACK_KING; p++)
        {
            U64 b = piece_bb[p];
            while (b)
                hash ^= Zobrist::psq[p][popLsb(b)];
        }

        if (history[curr_ply].ep_sq != NO_SQUARE)
            hash ^= Zobrist::ep[fileOf(history[curr_ply].ep_sq)];
        hash ^= Zobrist::castle[history[curr_ply].castle_rights.getHashIndex()];
        hash ^= Zobrist::side;
        history[curr_ply].hash = hash;

        refreshAccumulator();
    }

    Board &Board::operator=(const Board &other)
    {
        if (this != &other)
        {
            curr_ply = other.curr_ply;
            stm = other.stm;
            hash = other.hash;

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
        hash = other.hash;

        std::copy(std::begin(other.piece_bb), std::end(other.piece_bb), std::begin(piece_bb));
        std::copy(std::begin(other.board), std::end(other.board), std::begin(board));
        std::copy(std::begin(other.history), std::end(other.history), std::begin(history));
        accumulators = other.accumulators;
    }

    void Board::print()
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
        std::cout << "Hash key: " << std::hex << hash << "\n\n";
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

    U64 Board::diagSliders(Color c) const
    {
        return piece_bb[makePiece(c, BISHOP)] | piece_bb[makePiece(c, QUEEN)];
    }

    U64 Board::orthSliders(Color c) const
    {
        return piece_bb[makePiece(c, ROOK)] | piece_bb[makePiece(c, QUEEN)];
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
        U64 attacks = pawnAttacks(~c, s) & piece_bb[makePiece(c, PAWN)];
        attacks |= getAttacks(KNIGHT, s, occ) & piece_bb[makePiece(c, KNIGHT)];
        attacks |= getAttacks(BISHOP, s, occ) & (piece_bb[makePiece(c, BISHOP)] | piece_bb[makePiece(c, QUEEN)]);
        attacks |= getAttacks(ROOK, s, occ) & (piece_bb[makePiece(c, ROOK)] | piece_bb[makePiece(c, QUEEN)]);
        attacks |= getAttacks(KING, s, occ) & piece_bb[makePiece(c, KING)];
        return attacks;
    }

    bool Board::nonPawnMat(Color c) const
    {
        return piece_bb[makePiece(c, KNIGHT)] | piece_bb[makePiece(c, BISHOP)] | 
               piece_bb[makePiece(c, ROOK)] | piece_bb[makePiece(c, QUEEN)];
    }

    // currently only supports quiet moves (used in movepicker)
    bool Board::givesCheck(const Move &m)
    {
        if (isCapture(m))
            return false;

        Square from = m.from();
        Square to = m.to();
        MoveType mt = m.type();
        Piece pc = pieceAt(from);

        assert(pc != NO_PIECE);
        assert(from >= a1 && from <= h8);
        assert(to >= a1 && to <= h8);
        assert(from != to);

        const Square opp_ksq = kingSq(~stm);
        assert(pieceAt(opp_ksq) == makePiece(~stm, KING));
        U64 occ = occupancy(WHITE) | occupancy(BLACK);

        if (mt == CASTLING)
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

            occ ^= SQUARE_BB[from] | SQUARE_BB[to];
            occ ^= SQUARE_BB[rook_from] | SQUARE_BB[rook_to];

            return SQUARE_BB[opp_ksq] & getAttacks(ROOK, rook_to, occ);
        }

        occ ^= SQUARE_BB[from] | SQUARE_BB[to];
        
        U64 attackers;
        U64 pc_from_bb = piece_bb[pc];

        if (mt == NORMAL)
        {
            piece_bb[pc] ^= SQUARE_BB[from] | SQUARE_BB[to];
            attackers = attackersTo(stm, opp_ksq, occ);
        }
        else // promotions
        {
            Piece prom_pc = makePiece(stm, typeOfPromotion(mt));
            U64 orig_prom_bb = piece_bb[prom_pc];

            piece_bb[pc] ^= SQUARE_BB[from];
            piece_bb[prom_pc] |= SQUARE_BB[to];

            attackers = attackersTo(stm, opp_ksq, occ);

            piece_bb[prom_pc] = orig_prom_bb;
        }

        piece_bb[pc] = pc_from_bb;

        return attackers;
    }

    bool Board::isLegal(const Move &m)
    {
        Square from = m.from();
        Square to = m.to();
        Square ksq = kingSq(stm);
        MoveType mt = m.type();
        Piece pc = pieceAt(from);
        Piece captured = pieceAt(to);

        assert(pc != NO_PIECE);
        assert(from >= a1 && from <= h8);
        assert(to >= a1 && to <= h8);
        assert(from != to);
        assert(typeOf(captured) != KING);
        assert(pieceAt(ksq) == makePiece(stm, KING));

        U64 pc_from_bb = piece_bb[pc];
        U64 pc_to_bb = piece_bb[captured];
        U64 occ = occupancy(WHITE) | occupancy(BLACK);

        if (mt == EN_PASSANT)
        {
            Square cap_sq = Square(to ^ 8);

            assert(board[cap_sq] == makePiece(~stm, PAWN));
            assert(pieceAt(to) == NO_PIECE);

            occ ^= SQUARE_BB[from] | SQUARE_BB[cap_sq];
            occ |= SQUARE_BB[to];

            piece_bb[pc] ^= SQUARE_BB[from] | SQUARE_BB[to];
            U64 all_attackers = attackersTo(~stm, ksq, occ);
            piece_bb[pc] = pc_from_bb;

            return !all_attackers;
        }

        if (mt == CASTLING)
        {
            to = relativeSquare(stm, to > from ? g1 : c1);
            Direction step = to > from ? WEST : EAST;

            for (Square s = to; s != from; s += step)
                if (attackersTo(~stm, ksq, occ))
                    return false;

            return true;
        }

        if (typeOf(pc) == KING)
            return !attackersTo(~stm, to, occ);

        occ ^= SQUARE_BB[from] | SQUARE_BB[to];
        piece_bb[pc] ^= SQUARE_BB[from] | SQUARE_BB[to];

        if (captured != NO_PIECE)
        {
            piece_bb[captured] ^= SQUARE_BB[to];
            occ ^= SQUARE_BB[to];
        }

        U64 all_attackers = attackersTo(~stm, ksq, occ);

        piece_bb[pc] = pc_from_bb;
        piece_bb[captured] = pc_to_bb;

        return !all_attackers;
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

        U64 curr_hash = hash ^ Zobrist::side;

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

            // move rook
            movePiece(rook_from, rook_to, update_nnue);        

            // update hash of rook
            Piece rook = pieceAt(rook_from);
            curr_hash ^= Zobrist::psq[rook][rook_from] ^ Zobrist::psq[rook][rook_to];
        } 
        
        if (captured != NO_PIECE) 
        {
            Square cap_sq = mt == EN_PASSANT ? Square(to ^ 8) : to;
            // remove captured piece from board
            removePiece(cap_sq, update_nnue);
            // remove captured piece from hash
            curr_hash ^= Zobrist::psq[captured][cap_sq];
        }
    
        // update hash of moving piece
        curr_hash ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

        // reset ep square if exists
        if (history[curr_ply].ep_sq != NO_SQUARE)
        {
            curr_hash ^= Zobrist::ep[fileOf(history[curr_ply].ep_sq)];
            history[curr_ply].ep_sq = NO_SQUARE;
        }

        // update castling rights
        if (history[curr_ply].castle_rights.onCastleSquare(from) || history[curr_ply].castle_rights.onCastleSquare(to))
        {
            curr_hash ^= Zobrist::castle[history[curr_ply].castle_rights.getHashIndex()];
            history[curr_ply].castle_rights.mask &= ~(SQUARE_BB[from] | SQUARE_BB[to]);
            curr_hash ^= Zobrist::castle[history[curr_ply].castle_rights.getHashIndex()];
        }

        // move piece 
        movePiece(from, to, update_nnue);

        if (pt == PAWN)
        {
            // set ep square if double push can be captured by enemy pawn
            auto ep_sq = Square(to ^ 8);
            if (std::abs(from - to) == 16 && (pawnAttacks(stm, ep_sq) & getPieceBB(~stm, PAWN))) 
            {
                history[curr_ply].ep_sq = ep_sq;
                // update hash
                curr_hash ^= Zobrist::ep[fileOf(ep_sq)];
            }
            else if (mt >= PR_KNIGHT)
            {
                PieceType prom_type = typeOfPromotion(mt);
                Piece prom_pc = makePiece(stm, prom_type);

                assert(prom_type != PAWN);
                assert(prom_pc != NO_PIECE);

                // add promoted piece and remove pawn
                removePiece(to, update_nnue);             
                putPiece(prom_pc, to, update_nnue);

                // update hash
                curr_hash ^= Zobrist::psq[pc][to] ^ Zobrist::psq[prom_pc][to];
            }
        }

        // set captured piece
        history[curr_ply].captured = captured;

        stm = ~stm;

        // set hash
        hash = curr_hash;
        history[curr_ply].hash = hash;

        Astra::tt.prefetch(hash);
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

        if (mt >= PR_KNIGHT)
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
        hash = history[curr_ply].hash;
    }

    void Board::makeNullMove()
    {
        curr_ply++;
        history[curr_ply] = StateInfo(history[curr_ply - 1]);

        if (history[curr_ply].ep_sq != NO_SQUARE) 
        {
            hash ^= Zobrist::ep[fileOf(history[curr_ply].ep_sq)];
            history[curr_ply].ep_sq = NO_SQUARE;
        }

        hash ^= Zobrist::side;

        history[curr_ply].half_move_clock++;
        history[curr_ply].hash = hash;
        
        stm = ~stm;

        Astra::tt.prefetch(hash);
    }

    void Board::unmakeNullMove()
    {
        curr_ply--;
        hash = history[curr_ply].hash;
        stm = ~stm;
    }

    bool Board::isRepetition(bool is_pv) const
    {
        int count = 0;
        int limit = curr_ply - history[curr_ply].half_move_clock - 1;

        for (int i = curr_ply - 2; i >= 0 && i >= limit; i -= 2)
            if (history[i].hash == hash)
                if (++count == 1 + is_pv)
                    return true;

        return false;
    }

    bool Board::isInsufficientMaterial() const
    {
        U64 pawns = piece_bb[WHITE_PAWN] | piece_bb[BLACK_PAWN];
        U64 queens = piece_bb[WHITE_QUEEN] | piece_bb[BLACK_QUEEN];
        U64 rooks = piece_bb[WHITE_ROOK] | piece_bb[BLACK_ROOK];
        int num_w_minor_pieces = popCount(piece_bb[WHITE_KNIGHT] | piece_bb[WHITE_BISHOP]);
        int num_b_minor_pieces = popCount(piece_bb[BLACK_KNIGHT] | piece_bb[BLACK_BISHOP]);
        // draw when KvK, KvK+B, KvK+N, K+NvK+N, K+BvK+B
        return !pawns && !queens && !rooks && num_w_minor_pieces <= 1 && num_b_minor_pieces <= 1;
    }

    // doesn't include stalemate or threefold
    bool Board::isDraw() const
    {
        return history[curr_ply].half_move_clock > 99 || isInsufficientMaterial();
    }

    bool Board::see(Move &move, int threshold)
    {
        if (move.type() !=  NORMAL)
            return true;

        Square from = move.from();
        Square to = move.to();
        PieceType attacker = typeOf(pieceAt(from));
        PieceType victim = typeOf(pieceAt(to));

        assert(from >= a1 && from <= h8);
        assert(to >= a1 && to <= h8);
        assert(from != to);
        assert(attacker != NO_PIECE_TYPE);
        
        int swap = PIECE_VALUES[victim] - threshold;
        if (swap < 0)
            return false;

        swap = PIECE_VALUES[attacker] - swap;

        if (swap <= 0)
            return true;

        U64 occ = occupancy(WHITE) | occupancy(BLACK);
        occ = occ ^ SQUARE_BB[from] ^ SQUARE_BB[to];
        U64 attackers = attackersTo(WHITE, to, occ) | attackersTo(BLACK, to, occ);

        U64 diag = diagSliders(WHITE) | diagSliders(BLACK);
        U64 orth = orthSliders(WHITE) | orthSliders(BLACK);

        bool result = true;

        Color stm = getTurn();

        U64 my_attacker, least_attacker;
        while (true)
        {
            stm = ~stm;
            attackers &= occ;

            if (!(my_attacker = (attackers & occupancy(stm))))
                break;
            result = !result;

            if ((least_attacker = my_attacker & getPieceBB(stm, PAWN)))
            {
                if ((swap = PIECE_VALUES[PAWN] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= getBishopAttacks(to, occ) & diag;
            }
            else if ((least_attacker = my_attacker & getPieceBB(stm, KNIGHT)))
            {
                if ((swap = PIECE_VALUES[KNIGHT] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
            }
            else if ((least_attacker = my_attacker & getPieceBB(stm, BISHOP)))
            {
                if ((swap = PIECE_VALUES[BISHOP] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= getBishopAttacks(to, occ) & diag;
            }
            else if ((least_attacker = my_attacker & getPieceBB(stm, ROOK)))
            {
                if ((swap = PIECE_VALUES[ROOK] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= getRookAttacks(to, occ) & orth;
            }
            else if ((least_attacker = my_attacker & getPieceBB(stm, QUEEN)))
            {
                if ((swap = PIECE_VALUES[QUEEN] - swap) < result)
                    break;
                occ ^= (least_attacker & -least_attacker);
                attackers |= (getBishopAttacks(to, occ) & diag) | (getRookAttacks(to, occ) & orth);
            }
            else
                return (attackers & ~occupancy(stm)) ? result ^ 1 : result;
        }

        return result;
    }

} // namespace Chess
