#ifndef BOARD_H
#define BOARD_H

#include <iostream>
#include "misc.h"
#include "zobrist.h"
#include "attacks.h"
#include "../eval/accumulator.h"

namespace Chess
{
    struct CastlingRights
    {
        U64 mask;

        CastlingRights() : mask(0) {}

        bool kingSide(const Color c) const
        {
            return (mask & ooMask(c)) == ooMask(c);
        }

        bool queenSide(const Color c) const
        {
            return (mask & oooMask(c)) == oooMask(c);
        }

        bool any(const Color c) const { return kingSide(c) || queenSide(c); }
        bool any() const { return any(WHITE) || any(BLACK); }

        bool onCastleSquare(Square s) const { return mask & SQUARE_BB[s]; }

        int getHashIndex() const
        {
            return kingSide(WHITE) + 2 * queenSide(WHITE) + 4 * kingSide(BLACK) + 8 * queenSide(BLACK);
        }
    };

    struct StateInfo
    {
        U64 hash;
        U64 pawn_hash;
        U64 non_pawn_hash[NUM_COLORS];
        Piece captured;
        Square ep_sq;
        CastlingRights castle_rights;
        U64 occ[NUM_COLORS] = {};
        U64 threats[NUM_PIECE_TYPES] = {};
        int half_move_clock;
        int plies_from_null;

        // enemy pieces that check our king
        U64 checkers = 0;
        // squares of our pinned pieces
        U64 pinned = 0;
        // potential danger squares for our king
        U64 danger = 0;

        StateInfo() = default;
        StateInfo(const StateInfo &other) = default;
        StateInfo &operator=(const StateInfo &other) = default;
    };

    class Board
    {
    public:
        StateInfo history[512];

        Board(const std::string &fen);
        Board(const Board &other);

        Board &operator=(const Board &other);

        void print() const;

        std::string getFen() const;

        U64 getPieceBB(Color c, PieceType pt) const;
        Piece pieceAt(Square s) const;
        Color getTurn() const;
        int getPly() const;
        int halfMoveClock() const;
        U64 getHash() const;
        U64 getPawnHash() const;
        U64 getNonPawnHash(Color c) const;
        U64 getThreats(PieceType pt) const;
        Square kingSq(Color c) const;
        NNUE::Accumulator &getAccumulator();
        void refreshAccumulator(Color c = BOTH_COLORS);
        void resetAccumulator();
        void resetPly();

        U64 diagSliders(Color c) const;
        U64 orthSliders(Color c) const;
        U64 occupancy(Color c) const;
        U64 attackersTo(Color c, Square s, U64 occ) const;
        U64 keyAfter(Move m) const;

        void makeMove(const Move &m, bool update_nnue = false);
        void unmakeMove(const Move &m);

        void makeNullMove();
        void unmakeNullMove();

        bool inCheck() const;
        bool nonPawnMat(Color c) const;
        bool isLegal(const Move &m) const;
        bool isPseudoLegal(const Move &m) const;

        bool isRepetition(int ply) const;
        bool isDraw(int ply) const;
        bool see(Move &m, int threshold) const;

        bool oppHasGoodCaptures() const;

    private:
        U64 piece_bb[NUM_PIECES];
        Piece board[NUM_SQUARES];
        Color stm;
        int curr_ply;
        NNUE::Accumulators accumulators;

        void putPiece(Piece p, Square s, bool update_nnue);
        void removePiece(Square s, bool update_nnue);
        void movePiece(Square from, Square to, bool update_nnue);

        void initThreats();
        void initCheckersAndPinned();
    };

    inline U64 Board::getPieceBB(Color c, PieceType pt) const
    {
        assert(pt != NO_PIECE_TYPE);
        return piece_bb[makePiece(c, pt)];
    }

    inline Piece Board::pieceAt(Square s) const { return board[s]; }
    inline Color Board::getTurn() const { return stm; }
    inline int Board::getPly() const { return curr_ply; }

    inline int Board::halfMoveClock() const { return history[curr_ply].half_move_clock; }

    inline U64 Board::getHash() const { return history[curr_ply].hash; }
    inline U64 Board::getPawnHash() const { return history[curr_ply].pawn_hash; }
    inline U64 Board::getNonPawnHash(Color c) const { return history[curr_ply].non_pawn_hash[c]; }

    inline Square Board::kingSq(Color c) const { return lsb(getPieceBB(c, KING)); }

    inline NNUE::Accumulator &Board::getAccumulator() { return accumulators.back(); }
    inline void Board::resetAccumulator() { accumulators.clear(); }

    inline void Board::resetPly() { curr_ply = 0; }

    inline U64 Board::occupancy(Color c = BOTH_COLORS) const
    {
        U64 occ = 0;
        if (c != BLACK)
            occ |= history[curr_ply].occ[WHITE];
        if (c != WHITE)
            occ |= history[curr_ply].occ[BLACK];

        return occ;
    }

    inline U64 Board::getThreats(PieceType pt) const
    {
        assert(pt != NO_PIECE_TYPE);
        return history[curr_ply].threats[pt];
    }

    inline U64 Board::diagSliders(Color c) const { return getPieceBB(c, BISHOP) | getPieceBB(c, QUEEN); }
    inline U64 Board::orthSliders(Color c) const { return getPieceBB(c, ROOK) | getPieceBB(c, QUEEN); }

    inline bool Board::inCheck() const { return history[curr_ply].checkers; }

    inline bool Board::oppHasGoodCaptures() const
    {
        const U64 queens = getPieceBB(stm, QUEEN);
        const U64 rooks = getPieceBB(stm, ROOK);
        const U64 minors = getPieceBB(stm, KNIGHT) | getPieceBB(stm, BISHOP);

        const U64 pawn_threats = getThreats(PAWN);
        const U64 minor_threats = pawn_threats | getThreats(KNIGHT) | getThreats(BISHOP);
        const U64 rook_threats = minor_threats | getThreats(ROOK);

        return (queens & rook_threats) | (rooks & minor_threats) | (minors & pawn_threats);
    }

    inline void Board::putPiece(Piece pc, Square psq, bool update_nnue)
    {
        assert(pc != NO_PIECE);

        board[psq] = pc;
        piece_bb[pc] |= SQUARE_BB[psq];
        history[curr_ply].occ[colorOf(pc)] ^= SQUARE_BB[psq];

        if (update_nnue)
            NNUE::nnue.putPiece(getAccumulator(), pc, psq, kingSq(WHITE), kingSq(BLACK));
    }

    inline void Board::removePiece(Square s, bool update_nnue)
    {
        Piece p = board[s];
        assert(p != NO_PIECE);

        piece_bb[p] ^= SQUARE_BB[s];
        board[s] = NO_PIECE;
        history[curr_ply].occ[colorOf(p)] ^= SQUARE_BB[s];

        if (update_nnue)
            NNUE::nnue.removePiece(getAccumulator(), p, s, kingSq(WHITE), kingSq(BLACK));
    }

    inline void Board::movePiece(Square from, Square to, bool update_nnue)
    {
        Piece pc = board[from];
        assert(pc != NO_PIECE);

        piece_bb[pc] ^= SQUARE_BB[from] | SQUARE_BB[to];
        board[to] = pc;
        board[from] = NO_PIECE;
        history[curr_ply].occ[colorOf(pc)] ^= SQUARE_BB[from] | SQUARE_BB[to];

        if (update_nnue)
        {
            const Square wksq = kingSq(WHITE);
            const Square bksq = kingSq(BLACK);

            if (typeOf(pc) == KING)
            {
                Square rel_from = relativeSquare(stm, from);
                Square rel_to = relativeSquare(stm, to);

                // refresh if different bucket index or king crossing the other half
                if (NNUE::KING_BUCKET[rel_from] != NNUE::KING_BUCKET[rel_to] || fileOf(from) + fileOf(to) == 7)
                {
                    refreshAccumulator(stm);
                    NNUE::nnue.movePiece(getAccumulator(), pc, from, to, wksq, bksq, ~stm);
                    return;
                }
            }

            NNUE::nnue.movePiece(getAccumulator(), pc, from, to, wksq, bksq);
        }
    }

} // namespace Chess

#endif // BOARD_H
