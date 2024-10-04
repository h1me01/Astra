#ifndef UCI_H
#define UCI_H

#include <unordered_map>
#include "search/search.h"
#include "options.h"

namespace UCI {

    class Uci {
    public :
        Uci();

        void loop();

    private:
        Board board;
        Astra::Search engine;

        std::unordered_map<std::string, Option> options;

        void printOptions() const;
        void applyOptions();
        void setOption(std::istringstream &is);
        std::string getOption(const std::string& str) const;

        void updatePosition(std::istringstream &is);
        void go(std::istringstream &is);

        Move getMove(const std::string &str_move) const;
    };

    inline Move Uci::getMove(const std::string &str_move) const {
        const Square from = findSquare(str_move.substr(0, 2));
        const Square to = findSquare(str_move.substr(2, 2));
        MoveFlags mf = QUIET;

        const Piece p_from = board.pieceAt(from);
        const Piece p_to = board.pieceAt(to);

        const bool is_capture = p_to != NO_PIECE;

        if (typeOf(p_from) == PAWN) {
            const bool is_double_push = (rankOf(from) == 1 && rankOf(to) == 3) || (rankOf(from) == 6 && rankOf(to) == 4);
            const bool is_promotion = rankOf(to) == 0 || rankOf(to) == 7;
            const bool is_ep = board.history[board.getPly()].ep_sq == to;

            if (is_double_push) {
                mf = DOUBLE_PUSH;
            } else if (is_ep) {
                mf = EN_PASSANT;
            } else if (is_promotion) {
                char promotion = tolower(str_move[4]);

                mf = is_capture ? PC_QUEEN : PR_QUEEN;
                if (promotion == 'r') {
                    mf = is_capture ? PC_ROOK : PR_ROOK;
                } else if (promotion == 'b') {
                    mf = is_capture ? PC_BISHOP : PR_BISHOP;
                } else if (promotion == 'n') {
                    mf = is_capture ? PC_KNIGHT : PR_KNIGHT;
                }
            } else if (is_capture) {
                mf = CAPTURE;
            }
        } else if (typeOf(p_from) == KING) {
            bool is_short = (from == e1 && to == g1) || (from == e8 && to == g8);
            bool is_long = (from == e1 && to == c1) || (from == e8 && to == c8);

            if (is_short) {
                mf = OO;
            } else if (is_long) {
                mf = OOO;
            } else if (is_capture) {
                mf = CAPTURE;
            }
        } else if (is_capture) {
            mf = CAPTURE;
        }

        return Move(from, to, mf);
    }

} // namespace UCI

#endif //UCI_H
