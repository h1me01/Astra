#include "movepicker.h"

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
        if (swap < 0)
            return false;
        swap -= PIECE_VALUES[attacker];
        if (swap >= 0)
            return true;

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

    // most valuable victim, least valuable attacker
    constexpr int mvvlva_table[7][7] = {
        {5, 4, 3, 2, 1, 0, 0},
        {15, 14, 13, 12, 11, 10, 0},
        {25, 24, 23, 22, 21, 20, 0},
        {35, 34, 33, 32, 31, 30, 0},
        {45, 44, 43, 42, 41, 40, 0},
        {55, 54, 53, 52, 51, 50, 0}};

    int mvvlva(const Board &board, Move move)
    {
        const int attacker = typeOf(board.pieceAt(move.from()));
        const int victim = typeOf(board.pieceAt(move.to()));
        return mvvlva_table[victim][attacker];
    }

    // movepicker class
    MovePicker::MovePicker(MoveType mt, const Search &search, const Stack *ss, Move tt_move)
        : mt(mt), search(search), ss(ss), ml(search.board, mt), tt_move(tt_move), idx(0) {}

    Move MovePicker::nextMove()
    {
        switch (stage)
        {
        case TT:
            stage++;

            if (tt_move != NO_MOVE && ml.find(tt_move) != -1)
                return tt_move;

            [[fallthrough]];
        case CAPTURES:
            stage++;
            scoreMoves();

            while (idx < ml.size())
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;

                // skip the tt move
                if (move == tt_move)
                    continue;

                return move;
            }

            // if we are in qsearch, then we are done
            if (mt == CAPTURE_MOVES)
                return NO_MOVE;

            [[fallthrough]];
        case KILLERS_1:
            stage++;

            if (killer1 != NO_MOVE) 
                return killer1;

            [[fallthrough]];
        case KILLERS_2:
            stage++;

            if (killer2 != NO_MOVE) 
                return killer2;

            [[fallthrough]];
        case COUNTER:
            stage++;

            if (counter_move != NO_MOVE) 
                return counter_move;

            [[fallthrough]];
        case QUIET:
            while (idx < ml.size())
            {
                partialInsertionSort(idx);
                Move move = ml[idx];
                idx++;

                // skip the tt, killers and counter move
                if (move == tt_move || move == killer1 || move == killer2 || move == counter_move)
                    continue;

                return move;
            }

            return NO_MOVE;
        default:
            assert(false); // we should never reach this point
            return NO_MOVE;
        }
    }

    // private member

    void MovePicker::scoreMoves()
    {
        for (int i = idx; i < ml.size(); i++)
        {
            if (ml[i] == tt_move)
                ml[i].score = TT_SCORE;
            else if (isCapture(ml[i]))
            {
                const int mvvlva_score = mvvlva(search.board, ml[i]);
                ml[i].score = see(search.board, ml[i], 0) ? CAPTURE_SCORE + mvvlva_score : mvvlva_score;
            }
            else if (ml[i] == search.history.getKiller1(ss->ply))
            {
                ml[i].score = KILLER_ONE_SCORE;
                killer1 = ml[i];
            }
            else if (ml[i] ==search.history.getKiller2(ss->ply))
            {
                ml[i].score = KILLER_TWO_SCORE;
                killer2 = ml[i];
            }
            else if (search.history.getCounterMove((ss - 1)->current_move) == ml[i])
            {
                ml[i].score = COUNTER_SCORE;
                counter_move = ml[i];
            }
            else 
            {
                Color stm = search.board.getTurn();
                ml[i].score = search.history.getHistoryScore(stm, ml[i]); 
            }
        }
    }

    void MovePicker::partialInsertionSort(int start)
    {
        int best_score = ml[start].score;
        int best_idx = start;

        for (int i = start + 1; i < ml.size(); i++)
        {
            if (ml[i].score > best_score)
            {
                best_score = ml[i].score;
                best_idx = i;
            }
        }

        std::swap(ml[start], ml[best_idx]);
    }

} // namespace Astra
