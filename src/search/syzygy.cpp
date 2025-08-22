#include "syzygy.h"
#include "../chess/movegen.h"

namespace Search {

unsigned int probe_wdl(const Board &board) {
    U64 w_occ = board.occupancy(WHITE);
    U64 b_occ = board.occupancy(BLACK);
    U64 occ = w_occ | b_occ;

    if(pop_count(occ) > signed(TB_LARGEST))
        return TB_RESULT_FAILED;

    U64 pawns = board.get_piecebb(WHITE, PAWN) | board.get_piecebb(BLACK, PAWN);
    U64 knights = board.get_piecebb(WHITE, KNIGHT) | board.get_piecebb(BLACK, KNIGHT);
    U64 bishops = board.get_piecebb(WHITE, BISHOP) | board.get_piecebb(BLACK, BISHOP);
    U64 rooks = board.get_piecebb(WHITE, ROOK) | board.get_piecebb(BLACK, ROOK);
    U64 queens = board.get_piecebb(WHITE, QUEEN) | board.get_piecebb(BLACK, QUEEN);
    U64 kings = board.get_piecebb(WHITE, KING) | board.get_piecebb(BLACK, KING);

    Square ep_sq = board.get_state().ep_sq;

    return tb_probe_wdl(                       //
        w_occ,                                 //
        b_occ,                                 //
        kings,                                 //
        queens,                                //
        rooks,                                 //
        bishops,                               //
        knights,                               //
        pawns,                                 //
        board.get_fmr(),                       //
        board.get_state().castle_rights.any(), //
        valid_sq(ep_sq) ? ep_sq : 0,           //
        board.get_stm() == WHITE               //
    );
}

} // namespace Search
