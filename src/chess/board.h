#pragma once

#include <memory>
#include "misc.h"
#include "zobrist.h"
#include "attacks.h"
#include "cuckoo.h"
#include "../net/accumulator.h"

namespace Chess
{
    struct CastlingRights
    {
        U64 mask;

        CastlingRights() : mask(0) {}

        bool kingSide(const Color c) const
        {
            assert(c == WHITE || c == BLACK);
            return (mask & OO_MASK[c]) == OO_MASK[c];
        }

        bool queenSide(const Color c) const
        {
            assert(c == WHITE || c == BLACK);
            return (mask & OOO_MASK[c]) == OOO_MASK[c];
        }

        bool any(const Color c) const { return kingSide(c) || queenSide(c); }
        bool any() const { return any(WHITE) || any(BLACK); }

        bool onCastleSquare(Square sq) const
        {
            assert(sq >= a1 && sq <= h8);
            return mask & SQUARE_BB[sq];
        }

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

        Board(const std::string &fen, bool update_nnue = true);
        Board(const Board &other);

        Board &operator=(const Board &other);

        void print() const;

        std::string getFen() const;

        U64 getPieceBB(Color c, PieceType pt) const;
        Piece pieceAt(Square sq) const;
        Color getTurn() const;
        int getPly() const;
        int halfMoveClock() const;
        U64 getHash() const;
        U64 getPawnHash() const;
        U64 getNonPawnHash(Color c) const;
        U64 getThreats(PieceType pt) const;
        Square kingSq(Color c) const;
        NNUE::Accum &getAccumulator();

        void resetAccumulator();
        void resetPly();

        U64 diagSliders(Color c) const;
        U64 orthSliders(Color c) const;
        U64 occupancy(Color c) const;
        U64 attackersTo(Color c, Square sq, U64 occ) const;
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
        bool hasUpcomingRepetition(int ply);

        int getPhase() const;

    private:
        U64 piece_bb[NUM_PIECES];
        Piece board[NUM_SQUARES];
        Color stm;
        int curr_ply;
        NNUE::Accumulators accumulators;
        std::unique_ptr<NNUE::AccumTable> accumulator_table = std::make_unique<NNUE::AccumTable>(NNUE::AccumTable());

        void putPiece(Piece pc, Square sq, bool update_nnue = false);
        void removePiece(Square sq, bool update_nnue = false);
        void movePiece(Square from, Square to, bool update_nnue = false);

        void initThreats();
        void initCheckersAndPinned();
    };

    inline U64 Board::getPieceBB(Color c, PieceType pt) const
    {
        assert(c == WHITE || c == BLACK);
        assert(pt >= PAWN && pt <= KING);
        return piece_bb[makePiece(c, pt)];
    }

    inline Piece Board::pieceAt(Square sq) const
    {
        assert(sq >= a1 && sq <= h8);
        return board[sq];
    }

    inline Color Board::getTurn() const { return stm; }
    inline int Board::getPly() const { return curr_ply; }

    inline int Board::halfMoveClock() const { return history[curr_ply].half_move_clock; }

    inline U64 Board::getHash() const { return history[curr_ply].hash; }
    inline U64 Board::getPawnHash() const { return history[curr_ply].pawn_hash; }

    inline U64 Board::getNonPawnHash(Color c) const
    {
        assert(c == WHITE || c == BLACK);
        return history[curr_ply].non_pawn_hash[c];
    }

    inline Square Board::kingSq(Color c) const { return lsb(getPieceBB(c, KING)); }

    inline NNUE::Accum &Board::getAccumulator() { return accumulators.back(); }

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
        assert(pt >= PAWN && pt <= KING);
        return history[curr_ply].threats[pt];
    }

    inline U64 Board::diagSliders(Color c) const { return getPieceBB(c, BISHOP) | getPieceBB(c, QUEEN); }
    inline U64 Board::orthSliders(Color c) const { return getPieceBB(c, ROOK) | getPieceBB(c, QUEEN); }

    inline bool Board::inCheck() const { return history[curr_ply].checkers; }

    inline int Board::getPhase() const
    {
        int phase = 3 * popCount(piece_bb[WHITE_KNIGHT] | piece_bb[BLACK_KNIGHT]);
        phase += 3 * popCount(piece_bb[WHITE_BISHOP] | piece_bb[BLACK_BISHOP]);
        phase += 5 * popCount(piece_bb[WHITE_ROOK] | piece_bb[BLACK_ROOK]);
        phase += 10 * popCount(piece_bb[WHITE_QUEEN] | piece_bb[BLACK_QUEEN]);
        return phase;
    }

    inline void Board::putPiece(Piece pc, Square sq, bool update_nnue)
    {
        assert(sq >= a1 && sq <= h8);
        assert(pc >= WHITE_PAWN && pc <= BLACK_KING);

        board[sq] = pc;
        piece_bb[pc] |= SQUARE_BB[sq];
        history[curr_ply].occ[colorOf(pc)] ^= SQUARE_BB[sq];

        if (update_nnue)
        {
            NNUE::Accum &acc = getAccumulator();
            NNUE::Accum &input = accumulators[accumulators.getIndex() - 1];

            NNUE::nnue.putPiece(acc, input, pc, sq, kingSq(WHITE), WHITE);
            NNUE::nnue.putPiece(acc, input, pc, sq, kingSq(BLACK), BLACK);
        }
    }

    inline void Board::removePiece(Square sq, bool update_nnue)
    {
        assert(sq >= a1 && sq <= h8);

        Piece pc = board[sq];
        assert(pc != NO_PIECE);

        piece_bb[pc] ^= SQUARE_BB[sq];
        board[sq] = NO_PIECE;
        history[curr_ply].occ[colorOf(pc)] ^= SQUARE_BB[sq];

        if (update_nnue)
        {
            NNUE::Accum &acc = getAccumulator();
            NNUE::Accum &input = accumulators[accumulators.getIndex() - 1];

            NNUE::nnue.removePiece(acc, input, pc, sq, kingSq(WHITE), WHITE);
            NNUE::nnue.removePiece(acc, input, pc, sq, kingSq(BLACK), BLACK);
        }
    }

    inline void Board::movePiece(Square from, Square to, bool update_nnue)
    {
        assert(from >= a1 && from <= h8);
        assert(to >= a1 && to <= h8);

        Piece pc = board[from];
        assert(pc != NO_PIECE);

        piece_bb[pc] ^= SQUARE_BB[from] | SQUARE_BB[to];
        board[to] = pc;
        board[from] = NO_PIECE;
        history[curr_ply].occ[colorOf(pc)] ^= SQUARE_BB[from] | SQUARE_BB[to];

        if (update_nnue)
        {
            NNUE::Accum &acc = getAccumulator();
            NNUE::Accum &input = accumulators[accumulators.getIndex() - 1];

            if (typeOf(pc) == KING)
            {
                Square rel_from = relSquare(stm, from);
                Square rel_to = relSquare(stm, to);

                // refresh if different bucket index or king crossing the other half
                if (NNUE::KING_BUCKET[rel_from] != NNUE::KING_BUCKET[rel_to] || fileOf(from) + fileOf(to) == 7)
                {
                    // other side doesn't need a refresh
                    NNUE::nnue.movePiece(acc, input, pc, from, to, kingSq(~stm), ~stm);
                    accumulator_table->refresh(stm, *this);
                    return;
                }
            }

            NNUE::nnue.movePiece(acc, input, pc, from, to, kingSq(WHITE), WHITE);
            NNUE::nnue.movePiece(acc, input, pc, from, to, kingSq(BLACK), BLACK);
        }
    }

} // namespace Chess
