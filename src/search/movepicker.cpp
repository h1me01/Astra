#include "movepicker.h"

namespace Astra
{
    void partialInsertionSort(MoveList<> &ml, int idx)
    {
        assert(idx >= 0);

        int best_idx = idx;
        for (int i = 1 + idx; i < ml.size(); i++)
            if (ml[i].score > ml[best_idx].score)
                best_idx = i;

        std::swap(ml[idx], ml[best_idx]);
    }

    // MovePicker class

    MovePicker::MovePicker(SearchType st, const Board &board, const History &history, const Stack *ss, const Move &tt_move, bool gen_checkers)
        : st(st), board(board), history(history), ss(ss), gen_checkers(gen_checkers)
    {
        if (st == PC_SEARCH)
            stage = GEN_NOISY;
        else if (st == Q_SEARCH)
        {
            if (board.inCheck())
            {
                stage = PLAY_TT_MOVE;
                this->tt_move = tt_move;
            }
            else
                stage = GEN_NOISY;
        }
        else
        {
            stage = PLAY_TT_MOVE;
            this->tt_move = tt_move;

            Move prev_move = (ss - 1)->curr_move;
            if (isValidMove(prev_move))
                counter = history.getCounterMove(prev_move);
            else
                counter = NO_MOVE;

            killer = ss->killer;
        }
    }

    Move MovePicker::nextMove()
    {
        switch (stage)
        {
        case PLAY_TT_MOVE:
            stage = GEN_NOISY;
            if (board.isPseudoLegal(tt_move))
                return tt_move;
            [[fallthrough]];
        case GEN_NOISY:
            idx = 0;
            stage = PLAY_NOISY;

            ml_main.gen<NOISY>(board);
            scoreNoisyMoves();

            [[fallthrough]];
        case PLAY_NOISY:
            while (idx < ml_main.size())
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;

                // skip tt move only when in negamax search
                if (move == tt_move)
                    continue;

                // we want to play captures first in qsearch, doesn't matter if its see is fails
                if (st != Q_SEARCH && !board.see(move, st == N_SEARCH ? -move.score / 32 : see_cutoff))
                {
                    ml_bad_noisy.add(move); // add to bad noisy
                    continue;
                }

                return move;
            }

            if (st == PC_SEARCH)
                return NO_MOVE; // no more moves

            if (st == Q_SEARCH && !board.inCheck())
            {
                if (!gen_checkers)
                    return NO_MOVE; // no more moves
                goto quiet_checkers;
            }

            // in evasion qsearch we can falltrough the killer and counter since we did not set them
            stage = PLAY_KILLER;
            [[fallthrough]];
        case PLAY_KILLER:
            stage = PLAY_COUNTER;
            if (!skip_quiets && killer != tt_move && board.isPseudoLegal(killer))
                return killer;
            [[fallthrough]];
        case PLAY_COUNTER:
            stage = GEN_QUIETS;
            if (!skip_quiets && counter != tt_move && counter != killer && board.isPseudoLegal(counter))
                return counter;
            [[fallthrough]];
        case GEN_QUIETS:
            idx = 0;
            stage = PLAY_QUIETS;

            if (!skip_quiets)
            {
                ml_main.gen<QUIETS>(board);
                scoreQuietMoves();
            }

            [[fallthrough]];
        case PLAY_QUIETS:
            while (idx < ml_main.size() && !skip_quiets)
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;

                // skip tt move, killer, and counter
                if (move == tt_move || move == killer || move == counter)
                    continue;

                return move;
            }

            if (st == Q_SEARCH)
                return NO_MOVE; // end of evasion qsearch

            idx = 0;
            stage = PLAY_BAD_NOISY;

            [[fallthrough]];
        case PLAY_BAD_NOISY:
            while (idx < ml_bad_noisy.size())
            {
                partialInsertionSort(ml_bad_noisy, idx);
                Move move = ml_bad_noisy[idx];
                idx++;
                return move;
            }

            return NO_MOVE; // no more moves
        case GEN_QUIET_CHECKERS:
        quiet_checkers:
            idx = 0;
            stage = PLAY_QUIET_CHECKERS;
            ml_main.gen<QUIET_CHECKERS>(board);
            [[fallthrough]];
        case PLAY_QUIET_CHECKERS:
            while (idx < ml_main.size())
            {
                partialInsertionSort(ml_main, idx);
                Move move = ml_main[idx];
                idx++;
                return move;
            }

            return NO_MOVE; // no more moves
        default:
            assert(false); // we should never reach this
            return NO_MOVE;
        }
    }

    // private member

    void MovePicker::scoreQuietMoves()
    {
        for (int i = 0; i < ml_main.size(); i++)
        {
            const Piece pc = board.pieceAt(ml_main[i].from());
            const PieceType pt = typeOf(pc);
            const Square from = ml_main[i].from();
            const Square to = ml_main[i].to();

            assert(to >= a1 && to <= h8);
            assert(from >= a1 && from <= h8);
            assert(pt >= PAWN && pt <= KING);
            assert(pc >= WHITE_PAWN && pc <= BLACK_KING);

            ml_main[i].score = 2 * history.getHH(board.getTurn(), ml_main[i]);
            ml_main[i].score += 2 * (int)(*(ss - 1)->conth)[pc][to];
            ml_main[i].score += (int)(*(ss - 2)->conth)[pc][to];
            ml_main[i].score += (int)(*(ss - 4)->conth)[pc][to];
            ml_main[i].score += (int)(*(ss - 6)->conth)[pc][to];

            if (pt != PAWN && pt != KING)
            {
                U64 danger;
                if (pt == QUEEN)
                    danger = board.getThreats(ROOK);
                else if (pt == ROOK)
                    danger = board.getThreats(BISHOP) | board.getThreats(KNIGHT);
                else
                    danger = board.getThreats(PAWN);

                if (danger & SQUARE_BB[from])
                    ml_main[i].score += 16384 + 16384 * (pt == QUEEN);
                else if (danger & SQUARE_BB[to])
                    ml_main[i].score -= (16384 + 16384 * (pt == QUEEN));
            }
        }
    }

    void MovePicker::scoreNoisyMoves()
    {
        for (int i = 0; i < ml_main.size(); i++)
        {
            PieceType captured = ml_main[i].type() == EN_PASSANT ? PAWN : typeOf(board.pieceAt(ml_main[i].to()));

            int score;
            if (captured != NO_PIECE_TYPE)
                score = history.getCH(board, ml_main[i]);
            else
                // quiet queen prom is not a capture
                score = 0;

            ml_main[i].score = 16 * PIECE_VALUES[captured] + score;
        }
    }

} // namespace Astra