#include "syzygy.h"
#include "../fathom/tbprobe.h"
#include "../chess/movegen.h"

namespace Astra
{
    struct ProbeData
    {
        U64 w_occ;
        U64 b_occ;
        U64 occ;
        U64 pawns;
        U64 knights;
        U64 bishops;
        U64 rooks;
        U64 queens;
        U64 kings;
        int fmc; // fifty move counter
        int ep_sq;
        bool stm;
        bool any_castling;
        bool is_allowed = true;
    };

    ProbeData getProbeData(const Board &board)
    {
        ProbeData d;

        d.w_occ = board.occupancy(WHITE);
        d.b_occ = board.occupancy(BLACK);
        d.occ = d.w_occ | d.b_occ;

        if (popCount(d.occ) > signed(TB_LARGEST))
        {
            d.is_allowed = false;
            return d;
        }

        d.pawns = board.getPieceBB(WHITE, PAWN) | board.getPieceBB(BLACK, PAWN);
        d.knights = board.getPieceBB(WHITE, KNIGHT) | board.getPieceBB(BLACK, KNIGHT);
        d.bishops = board.getPieceBB(WHITE, BISHOP) | board.getPieceBB(BLACK, BISHOP);
        d.rooks = board.getPieceBB(WHITE, ROOK) | board.getPieceBB(BLACK, ROOK);
        d.queens = board.getPieceBB(WHITE, QUEEN) | board.getPieceBB(BLACK, QUEEN);
        d.kings = board.getPieceBB(WHITE, KING) | board.getPieceBB(BLACK, KING);

        int ply = board.getPly();
        d.fmc = board.history[ply].half_move_clock;
        d.any_castling = board.history[ply].castle_rights.any();

        Square ep_sq = board.history[ply].ep_sq;
        d.ep_sq = ep_sq != NO_SQUARE ? ep_sq : 0;

        d.ep_sq = board.history[ply].ep_sq;
        d.stm = board.getTurn() == WHITE;

        return d;
    }

    Score probeWDL(const Board &board)
    {
        ProbeData d = getProbeData(board);

        if (!d.is_allowed)
            return VALUE_NONE;

        unsigned wdl = tb_probe_wdl(
            d.w_occ, d.b_occ, d.kings, d.queens, d.rooks, d.bishops, d.knights, d.pawns, d.fmc, d.any_castling, d.ep_sq, d.stm);

        if (wdl == TB_LOSS)
            return -VALUE_TB_WIN;
        if (wdl == TB_WIN)
            return VALUE_TB_WIN;
        if (wdl == TB_BLESSED_LOSS || wdl == TB_CURSED_WIN || wdl == TB_DRAW)
            return VALUE_DRAW;

        // if probing failed return nothing
        return VALUE_NONE;
    }

    std::pair<Score, Move> probeDTZ(const Board &board)
    {
        ProbeData d = getProbeData(board);

        if (!d.is_allowed)
            return {VALUE_NONE, NO_MOVE};

        unsigned result = tb_probe_root(
            d.w_occ, d.b_occ, d.kings, d.queens, d.rooks, d.bishops, d.knights, d.pawns, d.fmc, d.any_castling, d.ep_sq, d.stm, NULL);

        // if the result failed don't do anything
        if (result == TB_RESULT_FAILED || result == TB_RESULT_CHECKMATE || result == TB_RESULT_STALEMATE)
            return {VALUE_NONE, NO_MOVE};

        int wdl = TB_GET_WDL(result);

        Score s = 0;
        if (wdl == TB_LOSS)
            s = -VALUE_TB_WIN_IN_MAX_PLY;
        if (wdl == TB_WIN)
            s = VALUE_TB_WIN_IN_MAX_PLY;
        if (wdl == TB_BLESSED_LOSS || wdl == TB_CURSED_WIN || wdl == TB_DRAW)
            s = VALUE_DRAW;

        const int prom_type = TB_GET_PROMOTES(result);
        const auto from = Square(TB_GET_FROM(result));
        const auto to = Square(TB_GET_TO(result));

        MoveList<> moves;
        moves.gen<LEGALS>(board);
        for (auto m : moves)
        {
            bool is_prom = typeOfPromotion(m.type()) == prom_type;

            if (from == m.from() && to == m.to() && (is_prom || !isProm(m)))
                return {s, m};
        }

        return {s, NO_MOVE};
    }

} // namespace Astra
