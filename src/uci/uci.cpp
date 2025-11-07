#include <algorithm> // std::all_of
#include <cctype>    // std::isdigit
#include <chrono>
#include <cstring> // strncmp

#include "../engine/threads.h"
#include "../engine/tune_params.h"
#include "../third_party/fathom/src/tbprobe.h"

#include "uci.h"

namespace UCI {

const std::string version = "6.2";

// bench positions from stockfish

// clang-format off
const std::vector<std::string> bench_positions = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 10",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 11",
    "4rrk1/pp1n3p/3q2pQ/2p1pb2/2PP4/2P3N1/P2B2PP/4RRK1 b - - 7 19",
    "rq3rk1/ppp2ppp/1bnpb3/3N2B1/3NP3/7P/PPPQ1PP1/2KR3R w - - 7 14 moves d4e6",
    "r1bq1r1k/1pp1n1pp/1p1p4/4p2Q/4Pp2/1BNP4/PPP2PPP/3R1RK1 w - - 2 14 moves g2g4",
    "r3r1k1/2p2ppp/p1p1bn2/8/1q2P3/2NPQN2/PPP3PP/R4RK1 b - - 2 15",
    "r1bbk1nr/pp3p1p/2n5/1N4p1/2Np1B2/8/PPP2PPP/2KR1B1R w kq - 0 13",
    "r1bq1rk1/ppp1nppp/4n3/3p3Q/3P4/1BP1B3/PP1N2PP/R4RK1 w - - 1 16",
    "4r1k1/r1q2ppp/ppp2n2/4P3/5Rb1/1N1BQ3/PPP3PP/R5K1 w - - 1 17",
    "2rqkb1r/ppp2p2/2npb1p1/1N1Nn2p/2P1PP2/8/PP2B1PP/R1BQK2R b KQ - 0 11",
    "r1bq1r1k/b1p1npp1/p2p3p/1p6/3PP3/1B2NN2/PP3PPP/R2Q1RK1 w - - 1 16",
    "3r1rk1/p5pp/bpp1pp2/8/q1PP1P2/b3P3/P2NQRPP/1R2B1K1 b - - 6 22",
    "r1q2rk1/2p1bppp/2Pp4/p6b/Q1PNp3/4B3/PP1R1PPP/2K4R w - - 2 18",
    "4k2r/1pb2ppp/1p2p3/1R1p4/3P4/2r1PN2/P4PPP/1R4K1 b - - 3 22",
    "3q2k1/pb3p1p/4pbp1/2r5/PpN2N2/1P2P2P/5PP1/Q2R2K1 b - - 4 26",
    "6k1/6p1/6Pp/ppp5/3pn2P/1P3K2/1PP2P2/3N4 b - - 0 1",
    "3b4/5kp1/1p1p1p1p/pP1PpP1P/P1P1P3/3KN3/8/8 w - - 0 1",
    "2K5/p7/7P/5pR1/8/5k2/r7/8 w - - 0 1 moves g5g6 f3e3 g6g5 e3f3",
    "8/6pk/1p6/8/PP3p1p/5P2/4KP1q/3Q4 w - - 0 1",
    "7k/3p2pp/4q3/8/4Q3/5Kp1/P6b/8 w - - 0 1",
    "8/2p5/8/2kPKp1p/2p4P/2P5/3P4/8 w - - 0 1",
    "8/1p3pp1/7p/5P1P/2k3P1/8/2K2P2/8 w - - 0 1",
    "8/pp2r1k1/2p1p3/3pP2p/1P1P1P1P/P5KR/8/8 w - - 0 1",
    "8/3p4/p1bk3p/Pp6/1Kp1PpPp/2P2P1P/2P5/5B2 b - - 0 1",
    "5k2/7R/4P2p/5K2/p1r2P1p/8/8/8 b - - 0 1",
    "6k1/6p1/P6p/r1N5/5p2/7P/1b3PP1/4R1K1 w - - 0 1",
    "1r3k2/4q3/2Pp3b/3Bp3/2Q2p2/1p1P2P1/1P2KP2/3N4 w - - 0 1",
    "6k1/4pp1p/3p2p1/P1pPb3/R7/1r2P1PP/3B1P2/6K1 w - - 0 1",
    "8/3p3B/5p2/5P2/p7/PP5b/k7/6K1 w - - 0 1",
    "5rk1/q6p/2p3bR/1pPp1rP1/1P1Pp3/P3B1Q1/1K3P2/R7 w - - 93 90",
    "4rrk1/1p1nq3/p7/2p1P1pp/3P2bp/3Q1Bn1/PPPB4/1K2R1NR w - - 40 21",
    "r3k2r/3nnpbp/q2pp1p1/p7/Pp1PPPP1/4BNN1/1P5P/R2Q1RK1 w kq - 0 16",
    "3Qb1k1/1r2ppb1/pN1n2q1/Pp1Pp1Pr/4P2p/4BP2/4B1R1/1R5K b - - 11 40",
    "4k3/3q1r2/1N2r1b1/3ppN2/2nPP3/1B1R2n1/2R1Q3/3K4 w - - 5 1",

    // 5-man positions
    "8/8/8/8/5kp1/P7/8/1K1N4 w - - 0 1",  // Kc2 - mate
    "8/8/8/5N2/8/p7/8/2NK3k w - - 0 1",   // Na2 - mate
    "8/3k4/8/8/8/4B3/4KB2/2B5 w - - 0 1", // draw

    // 6-man positions
    "8/8/1P6/5pr1/8/4R3/7k/2K5 w - - 0 1",  // Re5 - mate
    "8/2p4P/8/kr6/6R1/8/8/1K6 w - - 0 1",   // Ka2 - mate
    "8/8/3P3k/8/1p6/8/1P6/1K3n2 b - - 0 1", // Nd2 - draw

    // 7-man positions
    "8/R7/2q5/8/6k1/8/1P5p/K6R w - - 0 124", // Draw

    // Mate positions
    "6k1/3b3r/1p1p4/p1n2p2/1PPNpP1q/P3Q1p1/1R1RB1P1/5K2 b - - 0 1",
    "r2r1n2/pp2bk2/2p1p2p/3q4/3PN1QP/2P3R1/P4PP1/5RK1 w - - 0 1",
};
// clang-format on

// UCI

UCI::UCI() {
    std::cout << "Astra by Semih Oezalp" << std::endl;

    options.add("SyzygyPath", {OptionType::STRING, "", 0, 0});
    options.add("MoveOverhead", {OptionType::SPIN, "10", 1, 10000});
    options.add("MultiPV", {OptionType::SPIN, "1", 1, 218});
    options.add("Threads", {OptionType::SPIN, "1", 1, 128});
    options.add("Hash", {OptionType::SPIN, "16", 1, 8192});
}

void UCI::loop(int argc, char **argv) {
    if(argc > 1 && !strncmp(argv[1], "bench", 5)) {
        bench();
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
        else if(token == "ucinewgame")
            new_game();
        else if(token == "position")
            update_position(is);
        else if(token == "go")
            go(is);
        else if(token == "bench")
            bench();
        else if(token == "tune")
            Engine::params_to_spsa();
        else if(token == "setoption")
            options.set(is.str());
        else if(token == "d")
            board.print();
        else if(token == "stop") {
            Engine::threads.stop();
            Engine::threads.wait();
        } else if(token == "quit") {
            Engine::threads.stop();
            Engine::threads.wait();
            tb_free();
            break;
        } else
            std::cout << "Unknown Command: " << token << std::endl;
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
        std::cout << "Unknown command: " << token << std::endl;
        return;
    }

    board.set_fen(fen);
    while(is >> token) {
        if(token == "moves")
            continue;
        board.make_move(parse_move(token));
        // if half move clock gets reseted, then we can reset the history
        // since the last positions should not be considered in the repetition
        if(!board.get_fmr_count())
            board.reset_ply();
    }
}

void UCI::new_game() {
    Engine::tt.clear();
    Engine::threads.stop();
    Engine::threads.wait();
    Engine::threads.new_game();
}

void UCI::go(std::istringstream &is) {
    Engine::Limits limits;

    int64_t move_time = 0;
    int64_t w_time = 0, b_time = 0;

    int moves_to_go = 0;
    int w_inc = 0, b_inc = 0;

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
            std::cout << "Unknown command: " << token << "\n";
            return;
        }
    }

    Color stm = board.get_stm();
    int64_t time_left = (stm == WHITE) ? w_time : b_time;
    int inc = (stm == WHITE) ? w_inc : b_inc;

    if(move_time != 0) {
        limits.time.optimum = move_time;
        limits.time.maximum = move_time;
    } else if(time_left != 0) {
        limits.time = Engine::TimeMan::get_optimum( //
            time_left,                              //
            inc,                                    //
            std::max(moves_to_go, 0),               //
            std::stoi(options.get("MoveOverhead"))  //
        );
    }

    limits.multipv = std::stoi(options.get("MultiPV"));

    // start search
    Engine::threads.launch_workers(board, limits);
}

void UCI::bench() {
    new_game();
    U64 nodes = 0;

    auto start = std::chrono::high_resolution_clock::now();
    for(const auto &pos : bench_positions) {
        std::cout << "\ninput: " << pos << std::endl;

        std::istringstream iss("fen " + pos);
        update_position(iss);
        iss.clear();
        iss.str("depth 13");
        go(iss);

        Engine::threads.wait();

        nodes += Engine::threads.get_total_nodes();
    }

    Engine::threads.stop();

    auto end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << std::endl;
    std::cout << nodes << " nodes " << nodes * 1000 / total_time << " nps" << std::endl;
}

Move UCI::parse_move(const std::string &str_move) const {
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
