#ifndef BOARD_H
#define BOARD_H

#include <iostream>
#include "misc.h"
#include "zobrist.h"
#include "attacks.h"
#include "../eval/nnue.h"

namespace Chess
{
    struct StateInfo
    {
        U64 hash;
        Piece captured;
        Square ep_sq;
        U64 castle_mask;
        int half_move_clock;

        StateInfo() : hash(0), captured(NO_PIECE), ep_sq(NO_SQUARE), castle_mask(0), half_move_clock(0) {}

        StateInfo(const StateInfo& prev)
        {
            hash = prev.hash;
            captured = NO_PIECE;
            ep_sq = NO_SQUARE;
            castle_mask = prev.castle_mask;
            half_move_clock = prev.half_move_clock;
        }

        StateInfo& operator=(const StateInfo& other)
        {
            if (this != &other)
            {
                hash = other.hash;
                captured = other.captured;
                ep_sq = other.ep_sq;
                castle_mask = other.castle_mask;
                half_move_clock = other.half_move_clock;
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

        Board(const std::string& fen);
        Board(const Board& other);

        Board& operator=(const Board& other);

        void print(Color c) const;

        std::string getFen() const;

        U64 getPieceBB(Color c, PieceType pt) const;
        Piece pieceAt(Square s) const;
        Color getTurn() const;
        int getPly() const;
        int halfMoveClock() const;
        U64 getHash() const;
        Square kingSq(Color c) const;
        NNUE::Accumulator& getAccumulator();
        void refreshAccumulator();

        U64 occupancy(Color c) const;
        U64 attackers(Color c, Square s, U64 occ) const;
        U64 diagSliders(Color c) const;
        U64 orthSliders(Color c) const;

        void makeMove(const Move& m, bool update_nnue = false);
        void unmakeMove(const Move& m);

        void makeNullMove();
        void unmakeNullMove();

        bool inCheck() const;
        bool nonPawnMat(Color c) const;
        bool isCapture(const Move& m) const;
        bool givesCheck(const Move& m);

        bool isRepetition(bool is_pv) const;
        bool isInsufficientMaterial() const;
        bool isDraw() const;

        bool see(Move& move, int threshold);

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

    inline U64 Board::getPieceBB(Color c, PieceType pt) const
    {
        Piece piece = makePiece(c, pt);
        if (piece == NO_PIECE)
        {
            std::cerr << "Invalid piece type" << std::endl;
            return 0;
        }
        return piece_bb[piece];
    }

    inline Piece Board::pieceAt(Square s) const { return board[s]; }

    inline Color Board::getTurn() const { return stm; }

    inline int Board::getPly() const { return game_ply; }

    inline int Board::halfMoveClock() const { return history[game_ply].half_move_clock; }

    inline U64 Board::getHash() const { return hash; }

    inline Square Board::kingSq(Color c) const { return bsf(getPieceBB(c, KING)); }

    inline NNUE::Accumulator& Board::getAccumulator() { return accumulators.back(); }

    inline bool Board::inCheck() const
    {
        return attackers(~stm, kingSq(stm), occupancy(WHITE) | occupancy(BLACK));
    }

    inline bool Board::isCapture(const Move &m) const
    {
        return board[m.to()] != NO_PIECE || m.flag() == EN_PASSANT;
    }

    inline void Board::putPiece(Piece p, Square s, bool update_nnue)
    {
        board[s] = p;
        piece_bb[p] |= SQUARE_BB[s];

        if (update_nnue)
            NNUE::nnue.putPiece(getAccumulator(), p, s);
    }

    inline void Board::removePiece(Square s, bool update_nnue)
    {
        const Piece p = board[s];
        piece_bb[p] ^= SQUARE_BB[s];
        board[s] = NO_PIECE;

        if (update_nnue)
            NNUE::nnue.removePiece(getAccumulator(), p, s);
    }

    inline void Board::movePiece(Square from, Square to, bool update_nnue)
    {
        const Piece p = board[from];

        piece_bb[p] ^= SQUARE_BB[from] | SQUARE_BB[to];
        board[to] = p;
        board[from] = NO_PIECE;

        hash ^= Zobrist::psq[p][from];
        hash ^= Zobrist::psq[p][to];

        if (update_nnue)
            NNUE::nnue.movePiece(getAccumulator(), p, from, to);
    }

} // namespace Chess

#endif //BOARD_H
