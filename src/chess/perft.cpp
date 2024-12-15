#include <iostream>
#include <chrono>
#include "perft.h"
#include "movegen.h"

namespace Chess
{
    struct TestCase
    {
        std::string fen;
        std::vector<uint64_t> nodes;
    };

    // Positions from https://www.chessprogramming.org/Perft_Results
    std::vector<TestCase> test_cases = 
    {
        {
            STARTING_FEN, {20, 400, 8902, 197281, 4865609, 119060324, 3195901860, 84998978956}
        },
        {
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", {48, 2039, 97862, 4085603, 193690690, 8031647685}
        },
        {
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", {14, 191, 2812, 43238, 674624, 11030083, 178633661, 3009794393}
        },
        {
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {6, 264, 9467, 422333, 15833292, 706045033}
        },
        {
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379, 2103487, 89941194}
        },
        {
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {46, 2079, 89890, 3894594, 164075551, 6923051137, 287188994746, 11923589843526,}
        }
    };

    int num_ep_moves = 0;
    int num_pr_moves = 0;
    int num_q_moves = 0;
    int num_c_moves = 0;
    int num_cs_moves = 0;

    U64 perft(Board &board, int depth)
    {
        U64 nodes = 0;
        MoveList moves;
        moves.gen<LEGALS>(board);

        if (depth == 1)
        {
            /*
            for (auto &m : moves)
            {
                if (m.type() == EN_PASSANT)
                    num_ep_moves++;
                else if (m.type() == CASTLING)
                    num_cs_moves++;
                else if (isProm(m))
                    num_pr_moves++;
                else if (isCap(m))
                    num_c_moves++;
                else
                    num_q_moves++;
            }
            */
            return moves.size();
        }

        for (const Move &move : moves)
        {
            board.makeMove(move);
            nodes += perft(board, depth - 1);
            board.unmakeMove(move);
        }

        return nodes;
    }

    void testPerft(int max_depth)
    {
        double total_time = 0;

        for (const auto &testCase : test_cases)
        {
            std::cout << "\nFen: " << testCase.fen << std::endl;

            Board board(testCase.fen);

            for (size_t i = 0; i < testCase.nodes.size(); ++i)
            {
                const int depth = int(i) + 1;

                auto start = std::chrono::high_resolution_clock::now();
                auto nodes = perft(board, depth);
                auto end = std::chrono::high_resolution_clock::now();

                std::chrono::duration<double, std::milli> diff = end - start;
                std::cout << "Depth: " << depth << " | Nodes: " << nodes << " | Time: " << diff.count() << " | ";
                total_time += diff.count();

                if (nodes == testCase.nodes[i])
                {
                    std::cout << "Test passed" << std::endl;
                    /*
                    std::cout << "Number of en passant moves: " << num_ep_moves << std::endl;
                    std::cout << "Number of promotion moves: " << num_pr_moves << std::endl;
                    std::cout << "Number of castling moves: " << num_cs_moves << std::endl;
                    std::cout << "Number of capture moves: " << num_c_moves << std::endl;
                    std::cout << "Number of quiet moves: " << num_q_moves << std::endl;
                    num_ep_moves = num_pr_moves = num_cs_moves = num_c_moves = num_q_moves = 0;
                    */
                }
                else
                    std::cerr << "\033[31mTest failed\033[0m\n Expected nodes: " << testCase.nodes[i] << std::endl;

                if (depth >= max_depth)
                    break;
            }
        }

        std::cout << "\nTotal time: " << total_time << "ms" << std::endl;
    }

} // namespace Chess
