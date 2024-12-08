#ifndef BOARD_H
#define BOARD_H

#include <iostream>
#include "misc.h"
#include "zobrist.h"
#include "attacks.h"
#include "../eval/nnue.h"

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
        Piece captured;
        Square ep_sq;
        CastlingRights castle_rights;
        int half_move_clock;
        //U64 checkers;
        //U64 pinned;

        StateInfo() : hash(0), captured(NO_PIECE), ep_sq(NO_SQUARE), half_move_clock(0) {}

        StateInfo(const StateInfo &prev)
        {
            hash = prev.hash;
            captured = NO_PIECE;
            ep_sq = NO_SQUARE;
            half_move_clock = prev.half_move_clock;
            castle_rights = prev.castle_rights;
        }

        StateInfo &operator=(const StateInfo &other)
        {
            if (this != &other)
            {
                hash = other.hash;
                captured = other.captured;
                ep_sq = other.ep_sq;
                half_move_clock = other.half_move_clock;
                castle_rights = other.castle_rights;
            }
            return *this;
        }
    };

    class Board
    {
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

        Board(const std::string &fen);
        Board(const Board &other);

        Board &operator=(const Board &other);

        void print();

        std::string getFen() const;

        U64 getPieceBB(Color c, PieceType pt) const;
        Piece pieceAt(Square s) const;
        Color getTurn() const;
        int getPly() const;
        int halfMoveClock() const;
        U64 getHash() const;
        Square kingSq(Color c) const;
        NNUE::Accumulator &getAccumulator();
        void refreshAccumulator();

        U64 occupancy(Color c) const;
        U64 attackersTo(Color c, Square s, U64 occ) const;
        U64 diagSliders(Color c) const;
        U64 orthSliders(Color c) const;

        void makeMove(const Move &m, bool update_nnue = false);
        void unmakeMove(const Move &m);

        void makeNullMove();
        void unmakeNullMove();

        bool inCheck() const;
        bool nonPawnMat(Color c) const;
        bool isCap(const Move &m) const;
        bool givesCheck(const Move &m);
        bool isLegal(const Move &m);

        bool isRepetition(bool is_pv) const;
        bool isInsufficientMaterial() const;
        bool isDraw() const;

        bool see(Move &move, int threshold);

        //U64 getCheckers() const { return history[curr_ply].checkers; }
        //U64 getPinned() const { return history[curr_ply].pinned; }

    private:
        U64 piece_bb[NUM_PIECES];
        Piece board[NUM_SQUARES];
        Color stm;
        U64 hash;
        int curr_ply;
        NNUE::Accumulators accumulators;

        void putPiece(Piece p, Square s, bool update_nnue);
        void removePiece(Square s, bool update_nnue);
        void movePiece(Square from, Square to, bool update_nnue);

       // void initCheckersAndPinned();
    };

    inline U64 Board::getPieceBB(Color c, PieceType pt) const
    {
        assert(pt != NO_PIECE_TYPE);
        return piece_bb[makePiece(c, pt)];
    }

    inline Piece Board::pieceAt(Square s) const
    {
        assert(s >= a1 && s <= h8);
        return board[s];
    }

    inline Color Board::getTurn() const { return stm; }

    inline int Board::getPly() const { return curr_ply; }

    inline int Board::halfMoveClock() const { return history[curr_ply].half_move_clock; }

    inline U64 Board::getHash() const { return hash; }

    inline Square Board::kingSq(Color c) const { return bsf(getPieceBB(c, KING)); }

    inline NNUE::Accumulator &Board::getAccumulator() { return accumulators.back(); }

    inline bool Board::inCheck() const
    {
        return attackersTo(~stm, kingSq(stm), occupancy(WHITE) | occupancy(BLACK));
    }

    inline bool Board::isCap(const Move &m) const
    {
        return board[m.to()] != NO_PIECE || m.type() == EN_PASSANT;
    }

    inline void Board::putPiece(Piece p, Square s, bool update_nnue)
    {
        assert(p != NO_PIECE);
        assert(s >= a1 && s <= h8);

        board[s] = p;
        piece_bb[p] |= SQUARE_BB[s];

        if (update_nnue)
            NNUE::nnue.putPiece(getAccumulator(), p, s);
    }

    inline void Board::removePiece(Square s, bool update_nnue)
    {
        assert(s >= a1 && s <= h8);

        Piece p = board[s];
        assert(p != NO_PIECE);

        piece_bb[p] ^= SQUARE_BB[s];
        board[s] = NO_PIECE;

        if (update_nnue)
            NNUE::nnue.removePiece(getAccumulator(), p, s);
    }

    inline void Board::movePiece(Square from, Square to, bool update_nnue)
    {
        assert(from >= a1 && from <= h8);
        assert(to >= a1 && to <= h8);
        assert(from != to);

        Piece p = board[from];
        assert(p != NO_PIECE);

        piece_bb[p] ^= SQUARE_BB[from] | SQUARE_BB[to];
        board[to] = p;
        board[from] = NO_PIECE;

        if (update_nnue)
            NNUE::nnue.movePiece(getAccumulator(), p, from, to);
    }
    /*
    inline void Board::initCheckersAndPinned()
    {
        const Square ksq = kingSq(stm);
        const Color them = ~stm;
        const U64 their_occ = occupancy(them);
        const U64 our_occ = occupancy(stm);
     
        // enemy pawns attacks at our king
        history[curr_ply].checkers = pawnAttacks(stm, ksq) & getPieceBB(them, PAWN);
        // enemy knights attacks at our king
        history[curr_ply].checkers |= getAttacks(KNIGHT, ksq, our_occ | their_occ) & getPieceBB(them, KNIGHT);

        // potential enemy bishop, rook and queen attacks at our king
        U64 candidates = (getAttacks(ROOK, ksq, their_occ) & orthSliders(them)) | (getAttacks(BISHOP, ksq, their_occ) & diagSliders(them));

        history[curr_ply].pinned = 0;
        while (candidates)
        {
            Square s = popLsb(candidates);
            U64 blockers = SQUARES_BETWEEN[ksq][s] & our_occ;
            // if between the enemy slider attack and our king is no of our pieces
            // add the enemy piece to the checkers bitboard
            if (!blockers)
                history[curr_ply].checkers ^= SQUARE_BB[s];
            else if (sparsePopCount(blockers) == 1)
                // if there is only one of our piece between them, add our piece to the pinned
                history[curr_ply].pinned ^= blockers;
        }
    }*/

} // namespace Chess

#endif // BOARD_H
