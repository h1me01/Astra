#include "board.h"
#include <cassert>

namespace Chess {

    Board::Board(const std::string &fen) : checkers(0), pinned(0), danger(0), capture_mask(0), quiet_mask(0), piece_bb{0}, board{}, stm(WHITE), game_ply(0) {
        for (auto &i: board) {
            i = NO_PIECE;
        }
        history[0] = StateInfo();

        std::vector<std::string> fen_parts = split(fen, ' ');
        if (fen_parts.size() != 6) {
            std::cerr << "Invalid FEN string!" << std::endl;
            exit(1);
        }

        stm = fen_parts[1] == "w" ? WHITE : BLACK;

        history[game_ply].castle_mask = ALL_CASTLING_MASK;
        for (const char c: fen_parts[2]) {
            switch (c) {
                case 'K': history[game_ply].castle_mask &= ~WHITE_OO_MASK; break;
                case 'Q': history[game_ply].castle_mask &= ~WHITE_OOO_MASK; break;
                case 'k': history[game_ply].castle_mask &= ~BLACK_OO_MASK; break;
                case 'q': history[game_ply].castle_mask &= ~BLACK_OOO_MASK; break;
                default: break;
            }
        }

        history[game_ply].ep_sq = fen_parts[3] == "-" ? NO_SQUARE : findSquare(fen_parts[3]);
        history[game_ply].half_move_clock = std::stoi(fen_parts[4]);

        // place pieces to board
        int sqr = a8;
        for (const char c: fen_parts[0]) {
            if (isdigit(c))
                sqr += c - '0';
            else if (c == '/')
                sqr -= 16;
            else
                putPiece(static_cast<Piece>(PIECE_STR.find(c)), static_cast<Square>(sqr++), true);
        }

        // calculate hash
        hash = 0ULL;
        for (int p = WHITE_PAWN; p <= BLACK_KING; p++) {
            U64 bb = piece_bb[p];
            while (bb) {
                const Square s = popLsb(bb);
                hash ^= Zobrist::psq[p][s];
            }
        }

        hash ^= Zobrist::castle[getCastleHashIndex(history[game_ply].castle_mask)];
        hash ^= stm == WHITE ? Zobrist::side : 0;
        hash ^= history[game_ply].ep_sq == NO_SQUARE ? 0 : Zobrist::ep[fileOf(history[game_ply].ep_sq)];
        history[game_ply].hash = hash;
    }

    Board &Board::operator=(const Board &other) {
        if (this != &other) {
            checkers = other.checkers;
            pinned = other.pinned;
            danger = other.danger;
            capture_mask = other.capture_mask;
            quiet_mask = other.quiet_mask;
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

    void Board::print(Color c) const {
        for (int r = RANK_8; r >= RANK_1; --r) {
            for (int f = FILE_A; f <= FILE_H; ++f) {
                int s;
                if (c == WHITE) {
                    s = r * 8 + f;
                } else {
                    s = (7 - r) * 8 + (7 - f);
                }
                std::cout << PIECE_STR[board[s]] << " ";
            }
            std::cout << std::endl;
        }

        std::cout << "Fen: " << getFen() << std::endl;
        std::cout << "Hash key: " << hash << "\n\n";
    }

    std::string Board::getFen() const {
        std::ostringstream fen;

        for (int i = 56; i >= 0; i -= 8) {
            int empty = 0;

            for (int j = 0; j < 8; j++) {
                Piece p = board[i + j];
                if (p == NO_PIECE) {
                    empty++;
                } else {
                    fen << (empty == 0 ? "" : std::to_string(empty)) << PIECE_STR[p];
                    empty = 0;
                }
            }

            if (empty != 0) fen << empty;
            if (i > 0) fen << '/';
        }

        fen << (stm == WHITE ? " w " : " b ")
                << (history[game_ply].castle_mask & WHITE_OO_MASK ? "" : "K")
                << (history[game_ply].castle_mask & WHITE_OOO_MASK ? "" : "Q")
                << (history[game_ply].castle_mask & BLACK_OO_MASK ? "" : "k")
                << (history[game_ply].castle_mask & BLACK_OOO_MASK ? "" : "q")
                << (castleNotationHelper(fen) ? " " : "- ")
                << (history[game_ply].ep_sq == NO_SQUARE ? "-" : SQSTR[history[game_ply].ep_sq]);

        fen << " " << history[game_ply].half_move_clock << " " << (game_ply + 1) / 2;
        return fen.str();
    }

    void Board::refreshAccumulator() {
        accumulators.clear();
        std::vector<int16_t*> acc = accumulators.back();

        for (int j = 0; j < NNUE::HIDDEN_SIZE; j++) {
            acc[WHITE][j] = NNUE::nnue.fc1_biases[j];
            acc[BLACK][j] = NNUE::nnue.fc1_biases[j];
        }

        for(int i = WHITE_PAWN; i <= BLACK_KING; i++) {
            uint64_t bb = piece_bb[i];
            while(bb) {
                const Square s = popLsb(bb);
                NNUE::nnue.putPiece(acc, static_cast<Piece>(i), s);
            }
        }
    }

    U64 Board::diagSliders(Color c) const {
        return c == WHITE ? piece_bb[WHITE_BISHOP] | piece_bb[WHITE_QUEEN] : piece_bb[BLACK_BISHOP] | piece_bb[BLACK_QUEEN];
    }

    U64 Board::orthSliders(Color c) const {
        return c == WHITE ? piece_bb[WHITE_ROOK] | piece_bb[WHITE_QUEEN] : piece_bb[BLACK_ROOK] | piece_bb[BLACK_QUEEN];
    }

    U64 Board::occupancy(Color c) const {
        const int start_idx = (c == WHITE) ? 0 : 6;

        U64 occ = 0;
        for (int i = start_idx; i < start_idx + NUM_PIECE_TYPES; i++) {
            occ |= piece_bb[i];
        }
        return occ;
    }

    U64 Board::attackers(Color c, Square s, const U64 occ) const {
        const int pawn = (c == WHITE) ? WHITE_PAWN : BLACK_PAWN;
        const int knight = (c == WHITE) ? WHITE_KNIGHT : BLACK_KNIGHT;
        const int bishop = (c == WHITE) ? WHITE_BISHOP : BLACK_BISHOP;
        const int rook = (c == WHITE) ? WHITE_ROOK : BLACK_ROOK;
        const int queen = (c == WHITE) ? WHITE_QUEEN : BLACK_QUEEN;
        const Color opp_color = (c == WHITE) ? BLACK : WHITE;

        U64 attacks = pawnAttacks(opp_color, s) & piece_bb[pawn];
        attacks |= getAttacks(KNIGHT, s, occ) & piece_bb[knight];
        attacks |= getAttacks(BISHOP, s, occ) & (piece_bb[bishop] | piece_bb[queen]);
        attacks |= getAttacks(ROOK, s, occ) & (piece_bb[rook] | piece_bb[queen]);

        return attacks;
    }

    bool Board::inCheck() const {
        const U64 pieces = occupancy(WHITE) | occupancy(BLACK);
        return attackers(~stm, kingSq(stm), pieces);
    }

    bool Board::nonPawnMat(Color c) const {
        Piece knight = c == WHITE ? WHITE_KNIGHT : BLACK_KNIGHT;
        Piece bishop = c == WHITE ? WHITE_BISHOP : BLACK_BISHOP;
        Piece rook = c == WHITE ? WHITE_ROOK : BLACK_ROOK;
        Piece queen = c == WHITE ? WHITE_QUEEN : BLACK_QUEEN;
        return piece_bb[knight] | piece_bb[bishop] | piece_bb[rook] | piece_bb[queen];
    }

    void Board::makeMove(const Move &m, bool update_nnue) {
        const MoveFlags mf = m.flag();
        const Square from = m.from();
        const Square to = m.to();
        const Piece pc_from = board[from];
        const Piece pc_to = board[to];
        const PieceType pt = typeOf(pc_from);
        const U64 mask = SQUARE_BB[from] | SQUARE_BB[to];

        assert(from >= 0 && from < NUM_SQUARES);
        assert(to >= 0 && to < NUM_SQUARES);
        assert(typeOf(pc_to) != KING);
        assert(typeOf(pc_from) != NO_PIECE_TYPE);

        game_ply++;
        history[game_ply] = StateInfo(history[game_ply - 1]);
        history[game_ply].half_move_clock++;

        if (history[game_ply].ep_sq != NO_SQUARE) {
            hash ^= Zobrist::ep[fileOf(history[game_ply].ep_sq)];
        }
        history[game_ply].ep_sq = NO_SQUARE;

        hash ^= Zobrist::castle[getCastleHashIndex(history[game_ply].castle_mask)];

        // update castling rights
        history[game_ply].castle_mask |= mask;

        if (pt == PAWN || pc_to != NO_PIECE) {
            history[game_ply].half_move_clock = 0;
        }

        if (update_nnue) {
            accumulators.push();
        }

        if (mf == QUIET || mf == DOUBLE_PUSH || mf == EN_PASSANT) {
            const auto ep_sq = static_cast<Square>(to ^ 8);

            if (mf == DOUBLE_PUSH) {
                U64 ep_mask = pawnAttacks(stm, ep_sq);

                if (ep_mask & getPieceBB(~stm, PAWN)) {
                    history[game_ply].ep_sq = ep_sq;
                    hash ^= Zobrist::ep[fileOf(ep_sq)];
                }
            } else if (mf == EN_PASSANT) {
                hash ^= Zobrist::psq[makePiece(~stm, PAWN)][ep_sq];
                removePiece(static_cast<Square>(to ^ 8), update_nnue);
            }

            movePiece(from, to, update_nnue);
        } else if (mf == OO || mf == OOO) {
            Square rook_from, rook_to;

            if (mf == OO) {
                rook_from = stm == WHITE ? h1 : h8;
                rook_to = stm == WHITE ? f1 : f8;
            } else {
                rook_from = stm == WHITE ? a1 : a8;
                rook_to = stm == WHITE ? d1 : d8;
            }

            movePiece(from, to, update_nnue);
            movePiece(rook_from, rook_to, update_nnue);
        } else if (mf >= PR_KNIGHT && mf <= PC_QUEEN) {
            const PieceType prom_type = typeOfPromotion(mf);
            removePiece(from, update_nnue);

            if (mf >= PC_KNIGHT) {
                history[game_ply].captured = pc_to;
                hash ^= Zobrist::psq[pc_to][to];
                removePiece(to, update_nnue);
            }

            hash ^= Zobrist::psq[pc_from][from];
            hash ^= Zobrist::psq[makePiece(stm, prom_type)][to];

            putPiece(makePiece(stm, prom_type), to, update_nnue);
        } else if (mf == CAPTURE) {
            history[game_ply].captured = pc_to;
            hash ^= Zobrist::psq[pc_to][to];
            removePiece(to, update_nnue);
            movePiece(from, to, update_nnue);
        }

        hash ^= Zobrist::side;
        hash ^= Zobrist::castle[getCastleHashIndex(history[game_ply].castle_mask)];

        history[game_ply].hash = hash;

        stm = ~stm;
    }

    void Board::unmakeMove(const Move &m) {
        stm = ~stm;

        const MoveFlags mf = m.flag();
        const Square from = m.from();
        const Square to = m.to();

        if (accumulators.size()) {
            accumulators.pop();
        }

        if (mf == QUIET || mf == DOUBLE_PUSH || mf == EN_PASSANT) {
            movePiece(to, from, false);

            if (mf == EN_PASSANT) {
                putPiece(makePiece(~stm, PAWN), static_cast<Square>(to ^ 8), false);
            }
        } else if (mf == OO || mf == OOO) {
            Square rook_from, rook_to;

            if (mf == OO) {
                rook_from = stm == WHITE ? f1 : f8;
                rook_to = stm == WHITE ? h1 : h8;
            } else {
                rook_from = stm == WHITE ? d1 : d8;
                rook_to = stm == WHITE ? a1 : a8;
            }

            movePiece(to, from, false);
            movePiece(rook_from, rook_to, false);
        } else if (mf >= PR_KNIGHT && mf <= PC_QUEEN) {
            removePiece(to, false);
            putPiece(makePiece(stm, PAWN), from, false);

            if (mf >= PC_KNIGHT) {
                putPiece(history[game_ply].captured, to, false);
            }
        } else if (mf == CAPTURE) {
            movePiece(to, from, false);
            putPiece(history[game_ply].captured, to, false);
        }

        game_ply--;
        hash = history[game_ply].hash;
    }

    void Board::makeNullMove() {
        game_ply++;
        history[game_ply] = StateInfo(history[game_ply - 1]);

        hash ^= Zobrist::side;
        if(history[game_ply].ep_sq != NO_SQUARE) {
            hash ^= Zobrist::ep[fileOf(history[game_ply].ep_sq)];
        }
        history[game_ply].ep_sq = NO_SQUARE;

        history[game_ply].hash = hash;
        stm = ~stm;
    }

    void Board::unmakeNullMove() {
        game_ply--;
        hash = history[game_ply].hash;
        stm = ~stm;
    }

    bool Board::isThreefold() const {
        int count = 0;
        for (int i = game_ply - 1; i >= 0 && count < 2; i--) {
            count += (history[i].hash == hash);
        }
        return count >= 2;
    }

    bool Board::isFiftyMoveRule() const {
        return history[game_ply].half_move_clock >= 100;
    }

    bool Board::isInsufficientMaterial() const {
        const U64 pawns = piece_bb[WHITE_PAWN] | piece_bb[BLACK_PAWN];
        const U64 queens = piece_bb[WHITE_QUEEN] | piece_bb[BLACK_QUEEN];
        const U64 rooks = piece_bb[WHITE_ROOK] | piece_bb[BLACK_ROOK];
        const int num_w_minor_pieces = popCount(piece_bb[WHITE_KNIGHT] | piece_bb[WHITE_BISHOP]);
        const int num_b_minor_pieces = popCount(piece_bb[BLACK_KNIGHT] | piece_bb[BLACK_BISHOP]);
        // draw when KvK, KvK+B, KvK+N, K+NvK+N, K+BvK+B
        return !pawns && !queens && !rooks && num_w_minor_pieces <= 1 && num_b_minor_pieces <= 1;
    }

    bool Board::isDraw() const {
        return isFiftyMoveRule() || isThreefold() || isInsufficientMaterial();
    }

} // namespace Chess
