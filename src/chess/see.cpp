#include "board.h"

namespace Chess {

bool Board::see(Move &m, int threshold) const {
    assert(m);

    if(m.is_prom() || m.type() == EN_PASSANT || m.type() == CASTLING)
        return true;

    const StateInfo &info = states[curr_ply];

    const Square from = m.from();
    const Square to = m.to();
    const PieceType attacker = piece_type(piece_at(from));
    const PieceType victim = piece_type(piece_at(to));

    assert(attacker != NO_PIECE_TYPE);

    int swap = PIECE_VALUES_SEE[victim] - threshold;
    if(swap < 0)
        return false;

    swap = PIECE_VALUES_SEE[attacker] - swap;

    if(swap <= 0)
        return true;

    U64 occ = get_occupancy() ^ square_bb(from) ^ square_bb(to);
    U64 attackers = attackers_to(WHITE, to, occ) | attackers_to(BLACK, to, occ);

    const U64 diag = diag_sliders(WHITE) | diag_sliders(BLACK);
    const U64 orth = orth_sliders(WHITE) | orth_sliders(BLACK);

    int res = 1;

    Color curr_stm = get_stm();

    U64 stm_attacker, bb;
    while(true) {
        curr_stm = ~curr_stm;
        attackers &= occ;

        if(!(stm_attacker = (attackers & get_occupancy(curr_stm))))
            break;

        if(info.pinners[~stm] & occ) {
            stm_attacker &= ~info.blockers[stm];
            if(!stm_attacker)
                break;
        }

        res ^= 1;

        if((bb = stm_attacker & get_piecebb(curr_stm, PAWN))) {
            if((swap = PIECE_VALUES_SEE[PAWN] - swap) < res)
                break;
            occ ^= (bb & -bb);
            attackers |= get_attacks(BISHOP, to, occ) & diag;
        } else if((bb = stm_attacker & get_piecebb(curr_stm, KNIGHT))) {
            if((swap = PIECE_VALUES_SEE[KNIGHT] - swap) < res)
                break;
            occ ^= (bb & -bb);
        } else if((bb = stm_attacker & get_piecebb(curr_stm, BISHOP))) {
            if((swap = PIECE_VALUES_SEE[BISHOP] - swap) < res)
                break;
            occ ^= (bb & -bb);
            attackers |= get_attacks(BISHOP, to, occ) & diag;
        } else if((bb = stm_attacker & get_piecebb(curr_stm, ROOK))) {
            if((swap = PIECE_VALUES_SEE[ROOK] - swap) < res)
                break;
            occ ^= (bb & -bb);
            attackers |= get_attacks(ROOK, to, occ) & orth;
        } else if((bb = stm_attacker & get_piecebb(curr_stm, QUEEN))) {
            swap = PIECE_VALUES_SEE[QUEEN] - swap;
            assert(swap >= res);

            occ ^= (bb & -bb);
            attackers |= (get_attacks(BISHOP, to, occ) & diag) | (get_attacks(ROOK, to, occ) & orth);
        } else {
            return (attackers & ~get_occupancy(curr_stm)) ? res ^ 1 : res;
        }
    }

    return bool(res);
}

} // namespace Chess