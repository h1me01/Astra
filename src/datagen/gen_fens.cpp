#include <array>
#include <cstdlib>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "../ndarray.h"
#include "../search/threads.h"
#include "../search/types.h"
#include "../uci/uci.h"
#include "../util.h"
#include "gen_fens.h"

namespace astra::datagen {

size_t random_value(size_t max, std::mt19937_64& rng) { return std::uniform_int_distribution<size_t>{0, max - 1}(rng); }

search::Score evaluate_board(const std::string& fen, const search::Limits& limit) {
    Board board{fen};
    search::thread_pool.new_game();
    search::thread_pool.launch_workers(board, limit);
    search::thread_pool.wait();
    return search::thread_pool.main_thread()->best_root_move().score;
}

Board random_board(const std::string& startpos, const int num_moves, std::mt19937_64& rng) {
    MoveList<Move> ml;

    while (true) {
        Board board{startpos};
        bool valid = true;

        for (int i = 0; i < num_moves; ++i) {
            gen_moves<GenType::LEGAL>(ml, board);
            if (ml.empty()) {
                valid = false;
                break;
            }
            board.make_move(ml[random_value(ml.size(), rng)]);
        }

        if (!valid)
            continue;

        gen_moves<GenType::LEGAL>(ml, board);
        if (!ml.empty())
            return board;
    }
}

void generate_fens(int argc, char** argv) {
    const NDArray<int, 2> plies{4, 5};

    search::thread_pool.set_count(1);

    uint64_t num_fens, seed = 0;
    std::string book_path;

    if (argc < 2)
        return;

    std::string args = argv[1];
    std::istringstream iss(args);
    std::string token;

    while (iss >> token) {
        if (token == "genfens")
            iss >> num_fens;
        else if (token == "seed")
            iss >> seed;
        else if (token == "book")
            iss >> book_path;
    }

    std::mt19937_64 rng(seed);

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

    uint64_t counter = 0;
    while (counter < num_fens) {
        int moves_to_play = 8;
        std::string fen = uci::STARTING_FEN;

        if (!fens.empty()) {
            fen = fens[random_value(fens.size(), rng)];
            moves_to_play = plies(random_value(plies.total, rng));
        }

        Board board = random_board(fen, moves_to_play, rng);

        search::Limits limit;
        limit.depth = 10;
        limit.nodes = 1'000'000;
        limit.minimal = true;

        search::Score eval = evaluate_board(board.fen(), limit);
        if (std::abs(eval) <= 1000) {
            println("info string genfens {} ", board.fen());
            ++counter;
        }
    }
}

} // namespace astra::datagen
