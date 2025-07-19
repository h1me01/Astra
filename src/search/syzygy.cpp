#include "syzygy.h"
#include "../chess/movegen.h"
#include "../fathom/tbprobe.h"

namespace Astra {

struct ProbeData {
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

ProbeData getProbeData(const Board &board) {
    ProbeData d;

    d.w_occ = board.occupancy(WHITE);
    d.b_occ = board.occupancy(BLACK);
    d.occ = d.w_occ | d.b_occ;

    if(pop_count(d.occ) > signed(TB_LARGEST)) {
        d.is_allowed = false;
        return d;
    }

    d.pawns = board.get_piecebb(WHITE, PAWN) | board.get_piecebb(BLACK, PAWN);
    d.knights = board.get_piecebb(WHITE, KNIGHT) | board.get_piecebb(BLACK, KNIGHT);
    d.bishops = board.get_piecebb(WHITE, BISHOP) | board.get_piecebb(BLACK, BISHOP);
    d.rooks = board.get_piecebb(WHITE, ROOK) | board.get_piecebb(BLACK, ROOK);
    d.queens = board.get_piecebb(WHITE, QUEEN) | board.get_piecebb(BLACK, QUEEN);
    d.kings = board.get_piecebb(WHITE, KING) | board.get_piecebb(BLACK, KING);

    int ply = board.get_ply();
    d.fmc = board.halfmoveclock();
    d.any_castling = board.history[ply].castle_rights.any();

    Square ep_sq = board.history[ply].ep_sq;
    d.ep_sq = valid_sq(ep_sq) ? ep_sq : 0;

    d.stm = board.get_stm() == WHITE;

    return d;
}

Score probe_wdl(const Board &board) {
    ProbeData d = getProbeData(board);

    if(!d.is_allowed)
        return VALUE_NONE;

    unsigned wdl = tb_probe_wdl( //
        d.w_occ,                 //
        d.b_occ,                 //
        d.kings,                 //
        d.queens,                //
        d.rooks,                 //
        d.bishops,               //
        d.knights,               //
        d.pawns,                 //
        d.fmc,                   //
        d.any_castling,          //
        d.ep_sq,                 //
        d.stm                    //
    );

    if(wdl == TB_RESULT_FAILED)
        return VALUE_NONE;

    if(wdl == TB_LOSS)
        return VALUE_TB_LOSS;
    else if(wdl == TB_WIN)
        return VALUE_TB_WIN;
    else
        return VALUE_DRAW;
}

} // namespace Astra
