#include "moveordering.h"
#include "../chess/bitboard.h"

namespace Tsukuyomi {

    // static exchange evaluation
    bool see(const Board &board, Move move, int threshold) {
        Square from = move.from();
        Square to = move.to();
        PieceType attacker = typeOf(board.pieceAt(from));
        PieceType victim = typeOf(board.pieceAt(to));
        int swap = PIECE_VALUES[victim] - threshold;

        if (swap < 0) {
            return false;
        }

        swap -= PIECE_VALUES[attacker];

        if (swap >= 0) {
            return true;
        }

        U64 occ = board.occupancy(WHITE) | board.occupancy(BLACK);
        occ = occ ^ (1ULL << from) ^ (1ULL << to);
        U64 attackers = (board.attackers(WHITE, to, occ) | board.attackers(BLACK, to, occ)) & occ;

        U64 queens = board.getPieceBB(WHITE, QUEEN) | board.getPieceBB(BLACK, QUEEN);
        U64 rooks = board.getPieceBB(WHITE, ROOK) | board.getPieceBB(BLACK, ROOK) | queens;
        U64 bishops = board.getPieceBB(WHITE, BISHOP) | board.getPieceBB(BLACK, BISHOP) | queens;

        Color c_from = colorOf(board.pieceAt(from));
        Color c = ~c_from;

        while (true) {
            attackers &= occ;
            U64 my_attackers = attackers & board.occupancy(c);
            if (!my_attackers) {
                break;
            }

            int pt;
            for (pt = 0; pt <= 5; pt++) {
                U64 w_pieces = board.getPieceBB(WHITE, static_cast<PieceType>(pt));
                U64 b_pieces = board.getPieceBB(BLACK, static_cast<PieceType>(pt));

                if (my_attackers & (w_pieces | b_pieces)) {
                    break;
                }
            }

            c = ~c;

            if ((swap = -swap - 1 - PIECE_VALUES[pt]) >= 0) {
                if (pt == KING && (attackers & board.occupancy(c))) {
                    c = ~c;
                }
                break;
            }

            U64 w_pieces = board.getPieceBB(WHITE, static_cast<PieceType>(pt));
            U64 b_pieces = board.getPieceBB(BLACK, static_cast<PieceType>(pt));
            occ ^= (1ULL << (bsf(my_attackers & (w_pieces | b_pieces))));

            if (pt == PAWN || pt == BISHOP || pt == QUEEN) {
                attackers |= getBishopAttacks(to, occ) & bishops;
            }

            if (pt == ROOK || pt == QUEEN) {
                attackers |= getRookAttacks(to, occ) & rooks;
            }
        }

        return c != c_from;
    }

    // most valuable victim / least valuable attacker
    constexpr int mvvlvaTable[7][7] = {
        {205, 204, 203, 202, 201, 200, 0},
        {305, 304, 303, 302, 301, 300, 0},
        {405, 404, 403, 402, 401, 400, 0},
        {505, 504, 503, 502, 501, 500, 0},
        {605, 604, 603, 602, 601, 600, 0},
        {705, 704, 703, 702, 701, 700, 0}
    };

    int mvvlva(const Board &board, const Move &move) {
        const int attacker = typeOf(board.pieceAt(move.from()));
        const int victim = typeOf(board.pieceAt(move.to()));
        return mvvlvaTable[victim][attacker];
    }

    MoveOrdering::MoveOrdering() {
        for (int i = 0; i < MAX_PLY; ++i) {
            killer1[i] = NO_MOVE;
            killer2[i] = NO_MOVE;
        }

        clear();
    }

    void MoveOrdering::clear() {
        for (auto &counter: counters)
            for (auto &j: counter)
                j = NO_MOVE;

        for (auto &i: history)
            for (auto &j: i)
                for (int &k: j)
                    k = 0;
    }

    void MoveOrdering::updateKiller(Move &move, int ply) {
        killer2[ply] = killer1[ply];
        killer1[ply] = move;
    }

    void MoveOrdering::updateCounters(Move &move, Move &prev_move) {
        counters[prev_move.from()][prev_move.to()] = move;
    }

    void MoveOrdering::updateHistory(Board &board, Move &move, int bonus) {
        Color c = board.getTurn();
        Square from = move.from();
        Square to = move.to();

        int score = bonus - history[c][from][to] * std::abs(bonus) / 16384;
        history[c][from][to] += score;
    }

    void MoveOrdering::sortMoves(Board &board, MoveList &moves, Move& tt_move, Move& prev_move, int ply) const {
        std::vector<int> scores(moves.size(), 0);

        uint8_t move_count = 0;
        for (Move move: moves) {
            if (move == tt_move) {
                scores[move_count] = TT_SCORE;
            } else if (isCapture(move)) {
                const int mvvlvaScore = mvvlva(board, move);
                scores[move_count] = see(board, move, 0) ? CAPTURE_SCORE + mvvlvaScore : mvvlvaScore;
            } else if (move == killer1[ply]) {
                scores[move_count] = KILLER_ONE_SCORE;
            } else if (move == killer2[ply]) {
                scores[move_count] = KILLER_TWO_SCORE;
            } else if (getCounterMove(prev_move) == move) {
                scores[move_count] = COUNTER_SCORE;
            } else {
                scores[move_count] = getHistoryScore(board, move);
            }

            move_count++;
        }

        // bubble sort moves based on their scores
        int n = moves.size();
        bool swapped;

        do {
            swapped = false;
            for (int i = 0; i < n - 1; ++i) {
                if (scores[i] < scores[i + 1]) {
                    int temp_score = scores[i];
                    scores[i] = scores[i + 1];
                    scores[i + 1] = temp_score;

                    Move temp_move = moves[i];
                    moves[i] = moves[i + 1];
                    moves[i + 1] = temp_move;

                    swapped = true;
                }
            }

            n--;
        } while (swapped);
    }

    // private member

    int MoveOrdering::getHistoryScore(Board &board, Move &move) const {
        return history[board.getTurn()][move.from()][move.to()];
    }

    Move MoveOrdering::getCounterMove(Move &move) const {
        return counters[move.from()][move.to()];
    }

} // namespace Astra
