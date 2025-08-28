
#include <algorithm> // std::all_of
#include <cctype>    // std::isdigit
#include <cstring>   // strncmp

#include "bench.h"
#include "fathom/tbprobe.h"
#include "search/threads.h"
#include "search/tune_params.h"
#include "uci.h"

namespace UCI {

const std::string version = "6.1.1";

// helper

bool is_integer(const std::string &s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), ::isdigit);
}

// Options

void Options::print() const {
    for(const auto &elem : options) {
        Option option = elem.second;
        const std::string &value = option.default_val;

        std::cout << "option name " << elem.first //
                  << " type " << option.type      //
                  << " default " << (value.empty() ? "<empty>" : value);

        if(option.min != 0 && option.max != 0)
            std::cout << " min " << option.min << " max " << option.max << std::endl;
        else
            std::cout << std::endl;
    }

#ifdef TUNE
    for(auto param : Search::params)
        std::cout << "option name " << param->name         //
                  << " type spin default " << param->value //
                  << " min " << param->min                 //
                  << " max " << param->max << std::endl;
#endif
}

void Options::apply() {
    auto path = get("SyzygyPath");
    if(!path.empty() && path != "<empty>" && !use_tb) {
        bool success = tb_init(path.c_str());

        if(success && TB_LARGEST > 0) {
            use_tb = true;
            std::cout << "info string Successfully loaded syzygy path" << std::endl;
        } else {
            std::cout << "info string Failed to load syzygy path " << path << std::endl;
        }
    }

    worker_count = std::stoi(get("Threads"));
    Search::tt.set_worker_count(worker_count);
    Search::tt.init(std::stoi(get("Hash")));
}

void Options::set(std::istringstream &is) {
    std::vector<std::string> tokens = split(is.str(), ' ');
    const std::string &name = tokens[2];
    const std::string &value = tokens[4];

    if(tokens.size() < 5 || tokens[1] != "name" || tokens[3] != "value") {
        std::cout << "Invalid option command" << std::endl;
        return;
    }

    if(value == "<empty>" || value.empty())
        return;

#ifdef TUNE
    Search::set_param(name, std::stoi(value));
    Search::init_reductions();
#endif

    if(!options.count(name)) {
#ifndef TUNE
        std::cout << "Unknown option: " << name << std::endl;
#endif
        return;
    }

    if(options[name].type != "spin") {
        options[name] = value;
        return;
    }

    if(!is_integer(value)) {
        std::cout << "Invalid value for option " << name << std::endl;
        return;
    }

    int n = std::stoi(value);
    if(n >= options[name].min && n <= options[name].max)
        options[name] = value;
    else
        std::cout << "Invalid range of value for option " << name << std::endl;
}

// UCI

UCI::UCI() {
    std::cout << "Astra by Semih Oezalp" << std::endl;

    options.add("SyzygyPath", Option("string", "", "", 0, 0));
    options.add("MoveOverhead", Option("spin", "10", "10", 1, 10000));
    options.add("MultiPV", Option("spin", "1", "1", 1, 218));
    options.add("Threads", Option("spin", "1", "1", 1, 128));
    options.add("Hash", Option("spin", "16", "16", 1, 8192));

    options.apply();
}

void UCI::loop(int argc, char **argv) {
    if(argc > 1 && strncmp(argv[1], "bench", 5) == 0) {
        Bench::bench(13);
        return;
    }

    std::string line, token;
    while(std::getline(std::cin, line)) {
        std::istringstream is(line);
        token.clear();
        is >> std::skipws >> token;

        if(token == "uci") {
            std::cout << "id name Astra " << version << std::endl;
            std::cout << "id author Semih Oezalp" << std::endl;
            options.print();
            std::cout << "uciok" << std::endl;
        } else if(token == "isready")
            std::cout << "readyok" << std::endl;
        else if(token == "ucinewgame") {
            Search::tt.clear();
            Search::threads.force_stop();
        } else if(token == "position")
            update_position(is);
        else if(token == "go")
            go(is);
        else if(token == "bench")
            Bench::bench(13);
        else if(token == "tune")
            Search::params_to_spsa();
        else if(token == "setoption") {
            options.set(is);
            options.apply();
        } else if(token == "d")
            board.print();
        else if(token == "stop")
            Search::threads.force_stop();
        else if(token == "quit") {
            Search::threads.force_stop();
            tb_free();
            break;
        } else
            std::cout << "Unknown Command" << std::endl;
    }
}

void UCI::update_position(std::istringstream &is) {
    std::string token, fen;

    is >> token;
    if(token == "startpos")
        fen = STARTING_FEN;
    else if(token == "fen")
        while(is >> token && token != "moves")
            fen += token + " ";
    else {
        std::cout << "Unknown command" << std::endl;
        return;
    }

    board.set_fen(fen, false);
    while(is >> token) {
        if(token == "moves")
            continue;
        board.make_move(get_move(token), false);

        // if half move clock gets reseted, then we can reset the history
        // since the last positions should not be considered in the repetition
        if(board.get_fmr() == 0)
            board.reset_ply();
    }

    board.reset_accum_list();
}

void UCI::go(std::istringstream &is) {
    Search::threads.force_stop();
    Search::Limits limits;

    int64_t w_time = 0, b_time = 0, move_time = 0;
    int w_inc = 0, b_inc = 0, moves_to_go = 0;

    std::string token;
    while(is >> token) {
        if(token == "perft") {
            int depth;
            if(!(is >> depth))
                std::cout << "No depth value provided for perft\n";
            else
                board.perft(depth);

            return;
        } else if(token == "wtime")
            is >> w_time;
        else if(token == "btime")
            is >> b_time;
        else if(token == "winc")
            is >> w_inc;
        else if(token == "binc")
            is >> b_inc;
        else if(token == "movestogo")
            is >> moves_to_go;
        else if(token == "movetime")
            is >> move_time;
        else if(token == "depth")
            is >> limits.depth;
        else if(token == "nodes")
            is >> limits.nodes;
        else if(token == "infinite")
            limits.infinite = true;
        else {
            std::cout << "Unknown command\n";
            return;
        }
    }

    Color stm = board.get_stm();
    const int64_t time_left = (stm == WHITE) ? w_time : b_time;
    const int inc = (stm == WHITE) ? w_inc : b_inc;

    if(move_time != 0) {
        limits.time.optimum = move_time;
        limits.time.maximum = move_time;
    } else if(time_left != 0) {
        limits.time = Search::TimeMan::get_optimum( //
            time_left,                              //
            inc,                                    //
            std::max(moves_to_go, 0),               //
            std::stoi(options.get("MoveOverhead"))  //
        );
    }

    limits.multipv = std::stoi(options.get("MultiPV"));

    // start search
    Search::threads.launch_workers(board, limits, options.worker_count, options.use_tb);
}

Move UCI::get_move(const std::string &str_move) const {
    Square from = sq_from(str_move.substr(0, 2));
    Square to = sq_from(str_move.substr(2, 2));
    Piece pc = board.piece_at(from);
    Piece captured = board.piece_at(to);
    MoveType mt = QUIET;

    if(valid_piece(captured))
        mt = CAPTURE;

    if(piece_type(pc) == PAWN) {
        if(board.get_state().ep_sq == to)
            mt = EN_PASSANT;
        else if(sq_rank(to) == RANK_1 || sq_rank(to) == RANK_8) {
            char prom_t = tolower(str_move[4]); // piece type

            mt = valid_piece(captured) ? PC_QUEEN : PQ_QUEEN;
            if(prom_t == 'r')
                mt = MoveType(mt - 1);
            else if(prom_t == 'b')
                mt = MoveType(mt - 2);
            else if(prom_t == 'n')
                mt = MoveType(mt - 3);
        }
    } else if(piece_type(pc) == KING) {
        Color stm = board.get_stm();
        if(from == rel_sq(stm, e1) && (to == rel_sq(stm, g1) || to == rel_sq(stm, c1)))
            mt = CASTLING;
    }

    return Move(from, to, mt);
}

} // namespace UCI
