#ifndef MOVEORDERING_H
#define MOVEORDERING_H

#include "../chess/board.h"
#include "../chess/movegen.h"

using namespace Chess;

namespace Astra {

    const int PIECE_VALUES[] = {100, 325, 325, 500, 1000, 0, 0};

    // static exchange evaluation
    bool see(const Board &board, Move move, int threshold);

    enum MoveScores : int {
        TT_SCORE = 10'000'000,
        CAPTURE_SCORE = 7'000'000,
        KILLER_ONE_SCORE = 6'000'000,
        KILLER_TWO_SCORE = 5'000'000,
        COUNTER_SCORE = 4'000'000
    };

    class MoveOrdering {
    public:
        MoveOrdering();

        void clear();

        void updateKiller(Move &move, int ply);
        void updateCounters(Move& move, Move &prev_move);
        void updateHistory(Board &board, Move &move, int bonus);

        void sortMoves(Board &board, MoveList &moves, Move& tt_move, Move& prev_move, int ply) const;

    private:
        Move killer1[MAX_PLY];
        Move killer2[MAX_PLY];
        Move counters[NUM_SQUARES][NUM_SQUARES];

        int history[NUM_COLORS][NUM_SQUARES][NUM_SQUARES]{};

        int getHistoryScore(Board &board, Move &move) const;
        Move getCounterMove(Move &move) const;
    };

} // namespace Astra

#endif //MOVEORDERING_H
