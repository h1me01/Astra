#include "board.h"

namespace Chess {

bool Board::has_upcoming_repetition(int ply) {
    assert(ply > 0);

    const U64 occ = get_occupancy();

    const StateInfo &info = states[curr_ply];
    StateInfo *prev = &states[curr_ply - 1];

    int distance = std::min(info.fmr_counter, info.plies_from_null);
    for(int i = 3; i <= distance; i += 2) {
        prev -= 2;
        U64 move_key = info.hash ^ prev->hash;

        int hash = Cuckoo::cuckoo_h1(move_key);
        if(Cuckoo::keys[hash] != move_key)
            hash = Cuckoo::cuckoo_h2(move_key);

        if(Cuckoo::keys[hash] != move_key)
            continue;

        Move move = Cuckoo::cuckoo_moves[hash];
        Square from = move.from();
        Square to = move.to();

        U64 between = between_bb(from, to) ^ square_bb(to);
        if(between & occ)
            continue;

        if(ply > i)
            return true;

        Piece pc = piece_at(from) != NO_PIECE ? piece_at(from) : piece_at(to);
        if(piece_color(pc) != stm)
            continue;

        StateInfo *prev2 = prev - 2;
        for(int j = i + 4; j <= distance; j += 2) {
            prev2 -= 2;
            if(prev2->hash == prev->hash)
                return true;
        }
    }

    return false;
}

} // namespace Chess
