#ifndef BOARD_H
#define BOARD_H

#include "misc.h"
#include "zobrist.h"
#include "attacks.h"
#include "../eval/accumulator.h"
#include "../eval/nnue.h"
#include "../search/tt.h"

namespace Chess {

    struct StateInfo {
        U64 hash;
        Piece captured;
        Square ep_sq;
        U64 castle_mask;
        int half_move_clock;

        StateInfo() : hash(0), captured(NO_PIECE), ep_sq(NO_SQUARE), castle_mask(0), half_move_clock(0) {}

        StateInfo(const StateInfo &prev) {
            hash = prev.hash;
            captured = NO_PIECE;
            ep_sq = NO_SQUARE;
            castle_mask = prev.castle_mask;
            half_move_clock = prev.half_move_clock;
        }
    };

    class Board {
    public:
        StateInfo history[1024];
        // contains squares of enemy pieces that check our king
        U64 checkers;
        // contains squares of our pieces that are pinned
        U64 pinned;
        // contains potential danger squares for our king
        U64 danger;
        // contains all the possible capture squares
        U64 capture_mask;
        // contains all the possible squares that are not a capture
        U64 quiet_mask;

        explicit Board(const std::string &fen);

        Board &operator=(const Board &other);

        void print(Color c) const;

        std::string getFen() const;

        U64 getPieceBB(Color c, PieceType pt) const { return piece_bb[makePiece(c, pt)]; }
        Piece pieceAt(Square s) const { return board[s]; }
        Color getTurn() const { return stm; }
        int getPly() const { return game_ply; }
        U64 getHash() const { return hash; }
        Square kingSq(Color c) const { return bsf(getPieceBB(c, KING)); }
        std::vector<int16_t *> getAccumulator() const { return accumulators.back(); }
        void refreshAccumulator();

        U64 occupancy(Color c) const;
        U64 attackers(Color c, Square s, U64 occ) const;
        U64 diagSliders(Color c) const;
        U64 orthSliders(Color c) const;

        void makeMove(const Move &m, Astra::TTable* tt = nullptr, bool update_nnue = false);
        void unmakeMove(const Move &m);

        void makeNullMove();
        void unmakeNullMove();

        bool inCheck() const;
        bool nonPawnMat(Color c) const;

        bool isThreefold() const;
        bool isFiftyMoveRule() const;
        bool isInsufficientMaterial() const;
        bool isDraw() const;
    private:
        U64 piece_bb[NUM_PIECES];
        Piece board[NUM_SQUARES];
        Color stm;
        U64 hash;
        int game_ply;
        NNUE::Accumulators accumulators;

        void putPiece(Piece p, Square s, bool update_nnue);
        void removePiece(Square s, bool update_nnue);
        void movePiece(Square from, Square to, bool update_nnue);
    };

    inline void Board::putPiece(Piece p, Square s, bool update_nnue) {
        board[s] = p;
        piece_bb[p] |= SQUARE_BB[s];

        if (update_nnue) {
            std::vector<int16_t *> acc = accumulators.back();
            NNUE::nnue.putPiece(acc, p, s);
        }
    }

    inline void Board::removePiece(Square s, bool update_nnue) {
        const Piece p = board[s];
        piece_bb[p] &= ~SQUARE_BB[s];
        board[s] = NO_PIECE;

        if (update_nnue) {
            std::vector<int16_t *> acc = accumulators.back();
            NNUE::nnue.removePiece(acc, p, s);
        }
    }

    inline void Board::movePiece(Square from, Square to, bool update_nnue) {
        const Piece p = board[from];

        piece_bb[p] ^= SQUARE_BB[from] | SQUARE_BB[to];
        board[to] = p;
        board[from] = NO_PIECE;

        hash ^= Zobrist::psq[p][from];
        hash ^= Zobrist::psq[p][to];

        if (update_nnue) {
            std::vector<int16_t *> acc = accumulators.back();
            NNUE::nnue.movePiece(acc, p, from, to);
        }
    }

} // namespace Chess

#endif //BOARD_H
