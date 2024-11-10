#include "uci.h"
#include "bench.h"
#include "search/threads.h"
#include "chess/perft.h"
#include "search/tune.h"
#include "fathom/tbprobe.h"

#include <cstring> // strncmp

namespace UCI
{
    const std::string version = "4.0";

    // options class
    void Options::add(std::string name, const Option &option)
    {
        options[name] = option;
    }

    void Options::apply()
    {
        auto path = get("SyzygyPath");
        if (!path.empty() && path != "<empty>")
        {
            bool success = tb_init(path.c_str());

            if (success && TB_LARGEST > 0)
            {
                use_tb = true;
                std::cout << "info string successfully loaded syzygy path" << std::endl;
            }
            else
                std::cout << "info string failed to load syzygy path " << path << std::endl;
        }

        Astra::tt.init(std::stoi(get("Hash")));
        num_workers = std::stoi(get("Threads"));
    }

    void Options::set(std::istringstream &is)
    {
        std::vector<std::string> tokens = split(is.str(), ' ');
        const std::string &name = tokens[2];
        const std::string &value = tokens[4];

        if (tokens.size() < 5)
        {
            std::cout << "Invalid option command" << std::endl;
            return;
        }

        if (tokens[1] != "name")
        {
            std::cout << "Invalid option command";
            return;
        }

        if (tokens[3] != "value")
        {
            std::cout << "Invalid option command";
            return;
        }

        if (value == "<empty>")
            return;

#ifdef TUNE
        Astra::setParam(name, std::stoi(value));
        Astra::initReductions();
#endif

        if (options.count(name))
        {
            if (options[name].type == "spin") {
                int n_value = std::stoi(value);

                if (n_value >= options[name].min && n_value <= options[name].max)
                    options[name] = value;
                else
                    std::cout << "Invalid value for option: " << name << std::endl;
            } else {
                options[name] = value;
            }
        }
        else
        {
#ifndef TUNE
            std::cout << "Unknown option: " << name << std::endl;
#endif
        }
    }

    std::string Options::get(std::string str) const
    {
        auto it = options.find(str);
        if (it != options.end())
            return it->second.val;
        return "";
    }

    void Options::print() const
    {
        for (const auto &elem : options)
        {
            Option option = elem.second;
            const std::string &value = option.default_val;

            std::cout << "option name " << elem.first
                      << " type " << option.type
                      << " default " << (value.empty() ? "<empty>" : value);

            if (option.min != 0 && option.max != 0)
                std::cout << " min " << option.min << " max " << option.max << std::endl;
            else
                std::cout << std::endl;
        }

#ifdef TUNE
        Astra::paramsToUCI();
#endif
    }

    // uci class
    Uci::Uci() : board(STARTING_FEN)
    {
        options.add("Hash", Option("spin", "16", "16", 1, 2048));
        options.add("Threads", Option("spin", "1", "1", 1, 256));
#ifndef TUNE
        options.add("SyzygyPath", Option("string", "", "", 0, 0));
#endif
        options.apply();
    }

    void Uci::loop()
    {
        std::string line;
        std::string token;

        while (std::getline(std::cin, line))
        {
            std::istringstream is(line);
            token.clear();
            is >> std::skipws >> token;

            if (token == "uci")
            {
                std::cout << "id name Astra " << version << std::endl;
                std::cout << "id author Semih Oezalp" << std::endl;
                options.print();
                std::cout << "uciok" << std::endl;
            }
            else if (token == "isready")
                std::cout << "readyok" << std::endl;
            else if (token == "ucinewgame")
            {
                Astra::threads.stopAll();
                Astra::tt.clear();
            }
            else if (token == "position")
                updatePosition(is);
            else if (token == "go")
                go(is);
            else if (token == "bench")
            {
                Astra::threads.stop = false;
                Bench::bench(13);
            }
            else if (token == "tune")
            {
                Astra::paramsToSpsa();
                std::cout << std::endl;
                Astra::paramsToJSON();
            }
            else if (token == "setoption")
            {
                options.set(is);
                options.apply();
            }
            else if (token == "d")
                board.print(board.getTurn());
            else if (token == "stop")
                Astra::threads.stopAll();
            else if (token == "quit")
            {
                Astra::threads.stopAll();
                tb_free();
                break;
            }
            else
                std::cout << "Unknown Command" << std::endl;
        }
    }

    void Uci::updatePosition(std::istringstream &is)
    {
        std::string token, fen;

        is >> token;
        if (token == "startpos")
            fen = STARTING_FEN;
        else if (token == "fen")
            while (is >> token && token != "moves")
                fen += token + " ";
        else
        {
            std::cout << "Unknown command" << std::endl;
            return;
        }

        board = Board(fen);
        while (is >> token)
            if (token != "moves")
                board.makeMove(getMove(token));

        board.refreshAccumulator();
    }

    void Uci::go(std::istringstream &is)
    {
        Astra::threads.stopAll(); // stop all threads

        Astra::Limits limit;

        int64_t w_time = 0, b_time = 0, move_time = 0;
        int w_inc = 0, b_inc = 0, moves_to_go = 0;

        std::string token;
        while (is >> token)
        {
            if (token == "perft")
            {
                int depth;
                if (!(is >> depth))
                    std::cout << "No depth value provided.\n";
                else
                    testPerft(depth);
                return;
            }

            if (token == "wtime")
                is >> w_time;
            else if (token == "btime")
                is >> b_time;
            else if (token == "winc")
                is >> w_inc;
            else if (token == "binc")
                is >> b_inc;
            else if (token == "movestogo")
                is >> moves_to_go;
            else if (token == "movetime")
                is >> move_time;
            else if (token == "depth")
                is >> limit.depth;
            else if (token == "nodes")
                is >> limit.nodes;
            else if (token == "infinite")
                limit.infinite = true;
            else
            {
                std::cout << "Unknown command\n";
                return;
            }
        }

        Color stm = board.getTurn();
        const int64_t time_left = stm == WHITE ? w_time : b_time;
        const int inc = stm == WHITE ? w_inc : b_inc;

        if (move_time != 0)
            limit.time = move_time;
        else if (time_left != 0)
            limit.time = Astra::TimeManager::getOptimum(time_left, inc, moves_to_go);

        // start search
        Astra::threads.launchWorkers(board, limit, options.num_workers, options.use_tb);
    }

    Move Uci::getMove(const std::string &str_move) const
    {
        Square from = squareFromString(str_move.substr(0, 2));
        Square to = squareFromString(str_move.substr(2, 2));
        Piece p_from = board.pieceAt(from);
        MoveFlags mf = NORMAL;

        if (typeOf(p_from) == PAWN)
        {
            if (board.history[board.getPly()].ep_sq == to)
                mf = EN_PASSANT;
            else if (rankOf(to) == 0 || rankOf(to) == 7)
            {
                char promotion = tolower(str_move[4]);

                mf = PR_QUEEN;
                if (promotion == 'r')
                    mf = PR_ROOK;
                else if (promotion == 'b')
                    mf = PR_BISHOP;
                else if (promotion == 'n')
                    mf = PR_KNIGHT;
            }
        }
        else if (typeOf(p_from) == KING)
        {
            if ((from == e1 && to == g1) || (from == e8 && to == g8))
                mf = CASTLING;
            else if ((from == e1 && to == c1) || (from == e8 && to == c8))
                mf = CASTLING;
        }

        return Move(from, to, mf);
    }

} // namespace UCI
