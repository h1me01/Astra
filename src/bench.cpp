#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include "bench.h"
#include "search/search.h"

namespace Bench
{
    std::vector<std::string> bench_positions{
        "1r2k2r/3bb1pp/2n1pp2/1N6/2P1P3/2B5/4BPPP/3RR1K1 b - - 1 22",
        "1r2k2r/3b2pp/2n1pp2/1N6/1BP1P3/8/4BPPP/3RR1K1 b - - 0 23",
        "1r2k2r/3b2pp/4pp2/1N6/1nP1P3/8/3RBPPP/4R1K1 b - - 1 24",
        "1r5r/3bk1pp/4pp2/1N6/1nP1PP2/8/3RB1PP/4R1K1 b - - 0 25",
        "8/3rk1pp/2b5/R3p3/2P1P3/6P1/4BK1P/8 b - - 3 34",
        "8/3r2pp/2bk4/R1P1p3/4P3/6P1/4BK1P/8 b - - 0 35",
        "8/2kr2pp/2b5/R1P1p3/4P3/4K1P1/4B2P/8 b - - 2 36",
        "1k6/3r2pp/2b5/RBP1p3/4P3/4K1P1/7P/8 b - - 4 37",
        "5R2/2k3pp/2b5/2P1p3/4P3/r2B2P1/3K3P/8 b - - 14 42",
        "5R2/2k3pp/2b5/2P1p3/4P3/3BK1P1/r6P/8 b - - 16 43",
        "5R2/2k3pp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 18 44",
        "5R2/2k3pp/2b5/2P1p3/4P3/3B1KP1/r6P/8 b - - 20 45",
        "8/2k2Rpp/2b5/2P1p3/4P3/r2B1KP1/7P/8 b - - 22 46",
        "3k4/5Rpp/2b5/2P1p3/4P3/r2B2P1/4K2P/8 b - - 24 47",
        "8/1b1nk3/1r2p1pp/2r5/2BNpPPP/1P6/2P5/2K1R1R1 w - - 1 29",
        "8/1b2k3/1r2p1pp/2r1nP2/2BNp1PP/1P6/2P5/2K1R1R1 w - - 1 30",
        "8/1b2k3/1r2p1p1/2r1nPp1/2BNp2P/1P6/2P5/2K1R1R1 w - - 0 31",
        "8/1b2k3/1r2p1n1/2r3p1/2BNp2P/1P6/2P5/2K1R1R1 w - - 0 32",
        "8/4k3/3rp2R/6P1/2PN4/2P1p3/6b1/2K5 w - - 1 38",
        "8/4k3/r3p2R/2P3P1/3N4/2P1p3/6b1/2K5 w - - 1 39",
        "8/3k4/r3p2R/2P2NP1/8/2P1p3/6b1/2K5 w - - 3 40",
        "8/3k4/4p2R/2P3P1/8/2P1N3/6b1/r1K5 w - - 1 41",
        "r3r1k1/ppp3b1/4q1p1/3pP2p/P4P1B/R6P/1P4P1/3QR1K1 b - - 0 22",
        "r4rk1/ppp3b1/4q1p1/3pP1Bp/P4P2/R6P/1P4P1/3QR1K1 b - - 2 23",
        "r4rk1/pp4b1/4q1p1/2ppP1Bp/P4P2/3R3P/1P4P1/3QR1K1 b - - 1 24",
        "r5k1/pp3rb1/4q1p1/2p1P1B1/P2p1PP1/6R1/1P6/3QR1K1 b - - 2 27",
        "5rk1/pp3rb1/4q1p1/2p1P1B1/P2pRPP1/6R1/1P6/3Q2K1 b - - 4 28",
        "5rk1/1p3rb1/p3q1p1/P1p1P1B1/3pRPP1/6R1/1P6/3Q2K1 b - - 0 29",
        "4r1k1/1p3rb1/p3q1p1/P1p1P1B1/3pRPP1/1P4R1/8/3Q2K1 b - - 0 30",
        "4r1k1/5rb1/pP2q1p1/2p1P1B1/3pRPP1/1P4R1/8/3Q2K1 b - - 0 31",
        "4r1k1/5rb1/pq4p1/2p1P1B1/3pRPP1/1P4R1/4Q3/6K1 b - - 1 32",
        "4r1k1/1r4b1/pq4p1/2p1P1B1/3pRPP1/1P4R1/2Q5/6K1 b - - 3 33",
        "4r1k1/1r4b1/1q4p1/p1p1P1B1/3p1PP1/1P4R1/2Q5/4R1K1 b - - 1 34",
        "4r1k1/3r2b1/1q4p1/p1p1P1B1/2Qp1PP1/1P4R1/8/4R1K1 b - - 3 35",
        "r3r1k1/1q1nbppp/p1p1p3/1b6/1nBPPB2/1PN2N1P/4QPP1/3RR1K1 w - - 5 19",
        "r3rbk1/1q1n1ppp/p1p1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/4R1K1 w - - 7 20",
        "r3rbk1/1q3ppp/pnp1p3/1b6/1nBPPB2/1PN2N1P/3RQPP1/4R2K w - - 9 21",
        "2r1rbk1/1q4pp/pnp1pp2/1b6/1nBPPB2/1PN2N1P/4QPP1/1R1R3K w - - 0 23",
        "2r1rbk1/5qpp/pnp1pp2/1b6/1nBPP3/1PN1BN1P/4QPP1/1R1R3K w - - 2 24",
        "rr3bk1/5qp1/pnp1pp1p/1b6/2BPP3/1P2BN1P/Q4PP1/R2R2K1 w - - 3 28",
        "rr2qbk1/6p1/pnp1pp1p/1b6/2BPP3/1P2BN1P/4QPP1/R2R2K1 w - - 5 29",
        "rr2qbk1/6p1/1np1pp1p/pb6/2BPP3/1P1QBN1P/5PP1/R2R2K1 w - - 0 30",
        "rr2qbk1/6p1/1n2pp1p/pp6/3PP3/1P1QBN1P/5PP1/R2R2K1 w - - 0 31",
        "rr2qbk1/6p1/1n2pp1p/1p1P4/p3P3/1P1QBN1P/5PP1/R2R2K1 w - - 0 32",
        "rr2qbk1/3n2p1/3Ppp1p/1p6/p3P3/1P1QBN1P/5PP1/R2R2K1 w - - 1 33",
        "rr3bk1/3n2p1/3Ppp1p/1p5q/pP2P3/3QBN1P/5PP1/R2R2K1 w - - 1 34",
        "6k1/2b2p2/5pp1/p6p/3N1P1P/P1R3PK/r7/8 b - - 3 43",
        "6k1/5p2/1b3pp1/p6p/5P1P/P1R3PK/r1N5/8 b - - 5 44",
        "8/5pk1/1bR2pp1/p6p/5P1P/P5PK/r1N5/8 b - - 7 45",
        "3b4/5pk1/2R2pp1/p4P1p/7P/P5PK/r1N5/8 b - - 0 46",
        "8/8/2R2pk1/p6p/7P/4NK2/rb6/8 b - - 5 52",
    };

    void bench(int depth)
    {
        U64 nodes = 0;

        auto start = std::chrono::high_resolution_clock::now();

        for (const auto& fen : bench_positions)
        {
            std::cout << "======================================================" << std::endl;
            std::cout << "Position: " << fen << std::endl;
            std::cout << "======================================================" << std::endl;

            Astra::Search search(fen);
            search.limit.depth = depth;
            search.start();

            nodes += search.nodes;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        std::cout << "\n======================================================" << std::endl;
        std::cout << "Total Time (ms): " << total_time << std::endl;
        std::cout << "Total Nodes: " << nodes << std::endl;
        std::cout << "Nodes per second: " << nodes * 1000 / total_time << std::endl;
        std::cout << "======================================================" << std::endl;
    }

} // namespace Bench