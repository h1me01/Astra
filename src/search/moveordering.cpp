#include "moveordering.h"
#include "../chess/bitboard.h"

namespace Astra
{
    // static exchange evaluation from Weiss
    bool see(const Board &board, Move move, int threshold)
    {
        Square from = move.from();
        Square to = move.to();
        
        PieceType attacker = typeOf(board.pieceAt(from));
        PieceType victim = typeOf(board.pieceAt(to));
        
        int swap = PIECE_VALUES[victim] - threshold;
        if (swap < 0) return false;
        swap -= PIECE_VALUES[attacker];
        if (swap >= 0) return true;

        U64 occ = board.occupancy(WHITE) | board.occupancy(BLACK);
        occ = (occ ^ (1ULL << from)) | (1ULL << to);
        U64 attackers = (board.attackers(WHITE, to, occ) | board.attackers(BLACK, to, occ)) & occ;

        U64 queens = board.getPieceBB(WHITE, QUEEN) | board.getPieceBB(BLACK, QUEEN);
        U64 rooks = board.getPieceBB(WHITE, ROOK) | board.getPieceBB(BLACK, ROOK) | queens;
        U64 bishops = board.getPieceBB(WHITE, BISHOP) | board.getPieceBB(BLACK, BISHOP) | queens;

        Color c_from = colorOf(board.pieceAt(from));
        Color c = ~c_from;

        while (true)
        {
            attackers &= occ;

            U64 my_attackers = attackers & board.occupancy(c);
            if (!my_attackers)
                break;

            int pt;
            for (pt = 0; pt <= 5; pt++)
            {
                U64 w_pieces = board.getPieceBB(WHITE, static_cast<PieceType>(pt));
                U64 b_pieces = board.getPieceBB(BLACK, static_cast<PieceType>(pt));

                if (my_attackers & (w_pieces | b_pieces))
                    break;
            }

            c = ~c;

            if ((swap = -swap - 1 - PIECE_VALUES[pt]) >= 0)
            {
                if (pt == KING && (attackers & board.occupancy(c)))
                    c = ~c;
                break;
            }

            U64 w_pieces = board.getPieceBB(WHITE, static_cast<PieceType>(pt));
            U64 b_pieces = board.getPieceBB(BLACK, static_cast<PieceType>(pt));
            occ ^= (1ULL << (bsf(my_attackers & (w_pieces | b_pieces))));

            if (pt == PAWN || pt == BISHOP || pt == QUEEN)
                attackers |= getBishopAttacks(to, occ) & bishops;

            if (pt == ROOK || pt == QUEEN)
                attackers |= getRookAttacks(to, occ) & rooks;
        }

        return c != c_from;
    }

    // most valuable victim / least valuable attacker
    constexpr int mvvlva_table[7][7] = {
        {5, 4, 3, 2, 1, 0, 0},
        {15, 14, 13, 12, 11, 10, 0},
        {25, 24, 23, 22, 21, 20, 0},
        {35, 34, 33, 32, 31, 30, 0},
        {45, 44, 43, 42, 41, 40, 0},
        {55, 54, 53, 52, 51, 50, 0}};

    int mvvlva(const Board &board, const Move &move)
    {
        const int attacker = typeOf(board.pieceAt(move.from()));
        const int victim = typeOf(board.pieceAt(move.to()));
        return mvvlva_table[victim][attacker];
    }

    MoveOrdering::MoveOrdering()
    {
        clear();
    }

    void MoveOrdering::clear()
    {
        for (auto &i : history)
            for (auto &j : i)
                for (int &k : j)
                    k = 0;

        for (auto &counter : counters)
            for (auto &j : counter)
                j = NO_MOVE;

        for (int i = 0; i < MAX_PLY; ++i)
        {
            killer1[i] = NO_MOVE;
            killer2[i] = NO_MOVE;
        }
    }

    void MoveOrdering::update(Board &board, Move &move, Move &prev_move, int bonus, int ply)
    {
        counters[prev_move.from()][prev_move.to()] = move;

        if (!isCapture(move)) {
            killer2[ply] = killer1[ply];
            killer1[ply] = move;

            Color c = board.getTurn();
            Square from = move.from();
            Square to = move.to();

            int score = bonus - history[c][from][to] * std::abs(bonus) / 16384;
            history[c][from][to] += score;
        }
    }

    void MoveOrdering::sortMoves(Board &board, MoveList &moves, Move &tt_move, Move &prev_move, int ply) const
    {
        std::vector<int> scores(moves.size(), 0);

        uint8_t move_count = 0;
        for (Move move : moves)
        {
            if (move == tt_move)
                scores[move_count] = TT_SCORE;
            else if (isCapture(move))
            {
                const int mvvlva_score = mvvlva(board, move);
                scores[move_count] = see(board, move, 0) ? CAPTURE_SCORE + mvvlva_score : mvvlva_score;
            }
            else if (move == killer1[ply])
                scores[move_count] = KILLER_ONE_SCORE;
            else if (move == killer2[ply])
                scores[move_count] = KILLER_TWO_SCORE;
            else if (getCounterMove(prev_move) == move)
                scores[move_count] = COUNTER_SCORE;
            else
                scores[move_count] = getHistoryScore(board, move);

            move_count++;
        }

        // bubble sort moves based on their scores
        int n = moves.size();
        bool swapped;

        do
        {
            swapped = false;
            for (int i = 0; i < n - 1; ++i)
            {
                if (scores[i] < scores[i + 1])
                {
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

    int MoveOrdering::getHistoryScore(Board &board, Move &move) const
    {
        return history[board.getTurn()][move.from()][move.to()];
    }

    Move MoveOrdering::getCounterMove(Move &move) const
    {
        return counters[move.from()][move.to()];
    }

} // namespace Astra
