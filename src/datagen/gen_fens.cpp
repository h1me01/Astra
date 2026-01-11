#include <array>
#include <fstream>
#include <string>
#include <vector>

#include "../search/threads.h"
#include "gen_fens.h"

namespace datagen {

std::string to_lower(const std::string& str) {
    std::string lower_str = str;
    for (char& c : lower_str)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower_str;
}

Score evaluate_board(const std::string& fen, const search::Limits& limit) {
    Board board{fen};
    search::threads.new_game();
    search::threads.launch_workers(board, limit);
    search::threads.wait();
    return search::threads.main_thread()->get_best_move().score;
}

Board random_board(const std::string& startpos, const int num_moves) {
    Board board{startpos};
    MoveList<Move> legal_moves;

    for (int i = 0; i < num_moves; ++i) {
        legal_moves.gen<ADD_LEGALS>(board);
        if (!legal_moves.size())
            random_board(startpos, num_moves); // restart if no legal moves

        Move move = legal_moves[std::rand() % legal_moves.size()];
        board.make_move(move);
    }

    legal_moves.gen<ADD_LEGALS>(board);
    if (!legal_moves.size())
        return random_board(startpos, num_moves); // restart if no legal moves

    return board;
}

void generate_fens(int argc, char** argv) {
    constexpr std::array<int, 4> plies = {4, 5, 6, 7};

    search::threads.set_count(1);

    U64 num_fens, seed;
    std::string book_path;

    if (argc < 2)
        return;

    std::string args = argv[1];
    std::istringstream iss(args);
    std::string token;

    while (iss >> token) {
        std::cout << "Token : " << token << std::endl;
        if (token == "genfens")
            iss >> num_fens;
        else if (token == "seed")
            iss >> seed;
        else if (token == "book")
            iss >> book_path;
    }

    std::srand(seed);

    std::vector<std::string> fens;
    if (!book_path.empty() && to_lower(book_path) != "none") {
        std::ifstream book_file(book_path);
        if (book_file.is_open()) {
            std::string fen;
            while (std::getline(book_file, fen))
                fens.push_back(fen);
            book_file.close();
        }
    }

    U64 counter = 0;
    while (counter < num_fens) {
        int moves_to_play = 8;
        std::string fen = STARTING_FEN;

        if (!fens.empty()) {
            fen = fens[std::rand() % fens.size()];
            moves_to_play = plies[std::rand() % plies.size()];
        }

        Board board = random_board(fen, moves_to_play);

        search::Limits limit;
        limit.depth = 10;
        limit.minimal = true;

        Score eval = evaluate_board(board.fen(), limit);
        if (std::abs(eval) <= 800) {
            std::cout << "info string genfens " << board.fen() << std::endl;
            counter++;
        }
    }
}

} // namespace datagen
