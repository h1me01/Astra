#include <cassert>
#include "board.h"
#include "../search/search.h"

namespace Chess
{
    // helper

    // get castling hash index for the zobrist key
    int getCastleHashIndex(const U64 castle_mask)
    {
        bool wks = castle_mask & WHITE_OO_MASK;
        bool wqs = castle_mask & WHITE_OOO_MASK;
        bool bks = castle_mask & BLACK_OO_MASK;
        bool bqs = castle_mask & BLACK_OOO_MASK;
        return !wks + 2 * !wqs + 4 * !bks + 8 * !bqs;
    }

    // represent castle rights correctly in fen notation
    bool castleNotationHelper(const std::ostringstream &fen_stream)
    {
        const std::string fen = fen_stream.str();
        const std::string rights = fen.substr(fen.find(' ') + 1);
        return rights.find_first_of("kqKQ") != std::string::npos;
    }

    // board class

    Board::Board(const std::string &fen) : piece_bb{}, board{}, stm(WHITE), game_ply(0)
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

        history[game_ply].castle_mask = ALL_CASTLING_MASK;
        for (const char c : fen_parts[2])
        {
            switch (c)
            {
            case 'K':
                history[game_ply].castle_mask &= ~WHITE_OO_MASK;
                break;
            case 'Q':
                history[game_ply].castle_mask &= ~WHITE_OOO_MASK;
                break;
            case 'k':
                history[game_ply].castle_mask &= ~BLACK_OO_MASK;
                break;
            case 'q':
                history[game_ply].castle_mask &= ~BLACK_OOO_MASK;
                break;
            default:
                break;
            }
        }

        history[game_ply].ep_sq = fen_parts[3] == "-" ? NO_SQUARE : squareFromString(fen_parts[3]);
        history[game_ply].half_move_clock = std::stoi(fen_parts[4]);

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
            U64 bb = piece_bb[p];
            while (bb)
            {
                const Square s = popLsb(bb);
                hash ^= Zobrist::psq[p][s];
            }
        }

        if (history[game_ply].ep_sq == NO_SQUARE)
            hash ^= Zobrist::ep[fileOf(history[game_ply].ep_sq)];
        hash ^= Zobrist::side;
        hash ^= Zobrist::castle[getCastleHashIndex(history[game_ply].castle_mask)];
        history[game_ply].hash = hash;

        refreshAccumulator();
    }

    Board &Board::operator=(const Board &other)
    {
        if (this != &other)
        {
            game_ply = other.game_ply;
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
        game_ply = other.game_ply;
        stm = other.stm;
        hash = other.hash;

        std::copy(std::begin(other.piece_bb), std::end(other.piece_bb), std::begin(piece_bb));
        std::copy(std::begin(other.board), std::end(other.board), std::begin(board));
        std::copy(std::begin(other.history), std::end(other.history), std::begin(history));
        accumulators = other.accumulators;
    }

    void Board::print(Color c) const
    {
        std::cout << "\n +---+---+---+---+---+---+---+---+\n";

        for (int r = RANK_8; r >= RANK_1; --r)
        {
            for (int f = FILE_A; f <= FILE_H; ++f)
            {
                int s;
                if (c == WHITE)
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
            << (history[game_ply].castle_mask & WHITE_OO_MASK ? "" : "K")
            << (history[game_ply].castle_mask & WHITE_OOO_MASK ? "" : "Q")
            << (history[game_ply].castle_mask & BLACK_OO_MASK ? "" : "k")
            << (history[game_ply].castle_mask & BLACK_OOO_MASK ? "" : "q")
            << (castleNotationHelper(fen) ? " " : "- ")
            << (history[game_ply].ep_sq == NO_SQUARE ? "-" : SQSTR[history[game_ply].ep_sq]);

        fen << " " << history[game_ply].half_move_clock << " " << (game_ply == 0 ? 1 : (game_ply + 1) / 2);
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
            U64 bb = piece_bb[i];
            while (bb)
            {
                const Square s = popLsb(bb);
                NNUE::nnue.putPiece(acc, Piece(i), s);
            }
        }
    }

    U64 Board::diagSliders(Color c) const
    {
        Piece bishop = c == WHITE ? WHITE_BISHOP : BLACK_BISHOP;
        Piece queen = c == WHITE ? WHITE_QUEEN : BLACK_QUEEN;
        return piece_bb[bishop] | piece_bb[queen];
    }

    U64 Board::orthSliders(Color c) const
    {
        Piece rook = c == WHITE ? WHITE_ROOK : BLACK_ROOK;
        Piece queen = c == WHITE ? WHITE_QUEEN : BLACK_QUEEN;
        return piece_bb[rook] | piece_bb[queen];
    }

    U64 Board::occupancy(Color c) const
    {
        const int start_idx = (c == WHITE) ? 0 : 6;

        U64 occ = 0;
        for (int i = start_idx; i < start_idx + NUM_PIECE_TYPES; i++)
            occ |= piece_bb[i];

        return occ;
    }

    U64 Board::attackers(Color c, Square s, const U64 occ) const
    {
        Piece pawn = c == WHITE ? WHITE_PAWN : BLACK_PAWN;
        Piece knight = c == WHITE ? WHITE_KNIGHT : BLACK_KNIGHT;
        Piece bishop = c == WHITE ? WHITE_BISHOP : BLACK_BISHOP;
        Piece rook = c == WHITE ? WHITE_ROOK : BLACK_ROOK;
        Piece queen = c == WHITE ? WHITE_QUEEN : BLACK_QUEEN;
        Piece king = c == WHITE ? WHITE_KING : BLACK_KING;
        Color opp_color = c == WHITE ? BLACK : WHITE;

        U64 attacks = pawnAttacks(opp_color, s) & piece_bb[pawn];
        attacks |= getAttacks(KNIGHT, s, occ) & piece_bb[knight];
        attacks |= getAttacks(BISHOP, s, occ) & (piece_bb[bishop] | piece_bb[queen]);
        attacks |= getAttacks(ROOK, s, occ) & (piece_bb[rook] | piece_bb[queen]);
        attacks |= getAttacks(KING, s, occ) & piece_bb[king];

        return attacks;
    }

    bool Board::nonPawnMat(Color c) const
    {
        Piece knight = c == WHITE ? WHITE_KNIGHT : BLACK_KNIGHT;
        Piece bishop = c == WHITE ? WHITE_BISHOP : BLACK_BISHOP;
        Piece rook = c == WHITE ? WHITE_ROOK : BLACK_ROOK;
        Piece queen = c == WHITE ? WHITE_QUEEN : BLACK_QUEEN;
        return piece_bb[knight] | piece_bb[bishop] | piece_bb[rook] | piece_bb[queen];
    }

    bool Board::givesCheck(const Move& m) const 
    {
        return false;
    }

    void Board::makeMove(const Move &m, bool update_nnue)
    {
        const MoveFlags mf = m.flag();
        const Square from = m.from();
        const Square to = m.to();
        const Piece pc_from = board[from];
        const Piece pc_to = board[to];
        const PieceType pt = typeOf(pc_from);

        bool is_capture = pc_to != NO_PIECE;

        assert(from >= 0 && from < NUM_SQUARES);
        assert(to >= 0 && to < NUM_SQUARES);
        assert(typeOf(pc_to) != KING);
        assert(typeOf(pc_from) != NO_PIECE_TYPE);

        game_ply++;
        history[game_ply] = StateInfo(history[game_ply - 1]);
        history[game_ply].half_move_clock++;

        if (history[game_ply].ep_sq != NO_SQUARE)
        {
            hash ^= Zobrist::ep[fileOf(history[game_ply].ep_sq)];
            history[game_ply].ep_sq = NO_SQUARE;
        }

        hash ^= Zobrist::castle[getCastleHashIndex(history[game_ply].castle_mask)];

        // update castling rights
        history[game_ply].castle_mask |= SQUARE_BB[from] | SQUARE_BB[to];

        // reset half move clock if pawn move or capture
        if (pt == PAWN || pc_to != NO_PIECE)
            history[game_ply].half_move_clock = 0;

        if (update_nnue)
            accumulators.push();

        if (mf == CASTLING)
        {
            Square rook_from, rook_to;

            if (to == g1 || to == g8) // kingside
            {
                rook_from = stm == WHITE ? h1 : h8;
                rook_to = stm == WHITE ? f1 : f8;
            }
            else // queenside
            {
                rook_from = stm == WHITE ? a1 : a8;
                rook_to = stm == WHITE ? d1 : d8;
            }

            movePiece(from, to, update_nnue); // move king
            movePiece(rook_from, rook_to, update_nnue);
        }
        else if (mf >= PR_KNIGHT)
        {
            const PieceType prom_type = typeOfPromotion(mf);

            hash ^= Zobrist::psq[pc_from][from];
            removePiece(from, update_nnue);

            if (is_capture)
            {
                history[game_ply].captured = pc_to;
                hash ^= Zobrist::psq[pc_to][to];
                removePiece(to, update_nnue);
            }

            hash ^= Zobrist::psq[makePiece(stm, prom_type)][to];
            putPiece(makePiece(stm, prom_type), to, update_nnue);
        }
        else if (is_capture)
        {
            history[game_ply].captured = pc_to;
            hash ^= Zobrist::psq[pc_to][to];
            removePiece(to, update_nnue);
            movePiece(from, to, update_nnue);
        }
        else if (!is_capture || mf == EN_PASSANT)
        {
            const auto ep_sq = Square(to ^ 8);

            if (pt == PAWN && std::abs(from - to) == 16) // double push
            {
                U64 ep_mask = pawnAttacks(stm, ep_sq);

                if (ep_mask & getPieceBB(~stm, PAWN))
                {
                    history[game_ply].ep_sq = ep_sq;
                    hash ^= Zobrist::ep[fileOf(ep_sq)];
                }
            }
            else if (mf == EN_PASSANT)
            {
                hash ^= Zobrist::psq[makePiece(~stm, PAWN)][ep_sq];
                removePiece(ep_sq, update_nnue);
            }

            movePiece(from, to, update_nnue);
        }

        hash ^= Zobrist::side;
        hash ^= Zobrist::castle[getCastleHashIndex(history[game_ply].castle_mask)];

        history[game_ply].hash = hash;
        stm = ~stm;

        Astra::tt.prefetch(hash);
    }

    void Board::unmakeMove(const Move &m)
    {
        stm = ~stm;

        const MoveFlags mf = m.flag();
        const Square from = m.from();
        const Square to = m.to();

        const Piece captured = history[game_ply].captured;

        if (accumulators.size())
            accumulators.pop();

        if (mf == CASTLING)
        {
            Square rook_from, rook_to;

            if (to == g1 || to == g8) // kingside
            {
                rook_from = stm == WHITE ? f1 : f8;
                rook_to = stm == WHITE ? h1 : h8;
            }
            else // queenside
            {
                rook_from = stm == WHITE ? d1 : d8;
                rook_to = stm == WHITE ? a1 : a8;
            }

            movePiece(to, from, false); // move king
            movePiece(rook_from, rook_to, false);
        }
        else if (mf >= PR_KNIGHT)
        {
            removePiece(to, false);
            putPiece(makePiece(stm, PAWN), from, false);

            if (captured != NO_PIECE)
                putPiece(captured, to, false);
        }
        else if (captured != NO_PIECE)
        {
            movePiece(to, from, false);
            putPiece(captured, to, false);
        }
        else if (captured == NO_PIECE || mf == EN_PASSANT)
        {
            movePiece(to, from, false);

            if (mf == EN_PASSANT)
                putPiece(makePiece(~stm, PAWN), Square(to ^ 8), false);
        }

        game_ply--;
        hash = history[game_ply].hash;
    }

    void Board::makeNullMove()
    {
        game_ply++;
        history[game_ply] = StateInfo(history[game_ply - 1]);

        hash ^= Zobrist::side;
        if (history[game_ply].ep_sq != NO_SQUARE)
            hash ^= Zobrist::ep[fileOf(history[game_ply].ep_sq)];
        history[game_ply].ep_sq = NO_SQUARE;

        history[game_ply].hash = hash;
        stm = ~stm;

        Astra::tt.prefetch(hash);
    }

    void Board::unmakeNullMove()
    {
        game_ply--;
        hash = history[game_ply].hash;
        stm = ~stm;
    }

    bool Board::isRepetition(bool is_pv) const
    {
        int count = 0;
        int limit = game_ply - history[game_ply].half_move_clock - 1;

        for (int i = game_ply - 2; i >= 0 && i >= limit; i -= 2)
        {
            if (history[i].hash == hash)
            {
                if (++count == 1 + is_pv)
                {
                    return true;
                }
            }
        }

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
        return history[game_ply].half_move_clock > 99 || isInsufficientMaterial();
    }

    // static exchange evaluation from weiss
    bool Board::see(Move &move, int threshold)
    {
        Square from = move.from();
        Square to = move.to();
        PieceType attacker = typeOf(pieceAt(from));
        PieceType victim = typeOf(pieceAt(to));
        int swap = PIECE_VALUES[victim] - threshold;

        if (swap < 0)
            return false;

        swap -= PIECE_VALUES[attacker];

        if (swap >= 0)
            return true;

        U64 occ = occupancy(WHITE) | occupancy(BLACK);
        occ = occ ^ (1ULL << from) ^ (1ULL << to);
        U64 all_attackers = (attackers(WHITE, to, occ) | attackers(BLACK, to, occ)) & occ;

        U64 queens = getPieceBB(WHITE, QUEEN) | getPieceBB(BLACK, QUEEN);
        U64 rooks = getPieceBB(WHITE, ROOK) | getPieceBB(BLACK, ROOK) | queens;
        U64 bishops = getPieceBB(WHITE, BISHOP) | getPieceBB(BLACK, BISHOP) | queens;

        Color c_from = colorOf(pieceAt(from));
        Color c = ~c_from;

        while (true)
        {
            all_attackers &= occ;

            U64 my_attackers = all_attackers & occupancy(c);
            if (!my_attackers)
                break;

            int pt;
            for (pt = 0; pt <= 5; pt++)
            {
                U64 w_pieces = getPieceBB(WHITE, PieceType(pt));
                U64 b_pieces = getPieceBB(BLACK, PieceType(pt));

                if (my_attackers & (w_pieces | b_pieces))
                    break;
            }

            c = ~c;

            if ((swap = -swap - 1 - PIECE_VALUES[pt]) >= 0)
            {
                if (pt == KING && (all_attackers & occupancy(c)))
                    c = ~c;
                break;
            }

            U64 w_pieces = getPieceBB(WHITE, PieceType(pt));
            U64 b_pieces = getPieceBB(BLACK, PieceType(pt));
            occ ^= (1ULL << (bsf(my_attackers & (w_pieces | b_pieces))));

            if (pt == PAWN || pt == BISHOP || pt == QUEEN)
                all_attackers |= getBishopAttacks(to, occ) & bishops;

            if (pt == ROOK || pt == QUEEN)
                all_attackers |= getRookAttacks(to, occ) & rooks;
        }

        return c != c_from;
    }

} // namespace Chess
