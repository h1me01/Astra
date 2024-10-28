cmake_minimum_required(VERSION 3.28)
project(Astra)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(NNUE_FILE_PATH "${CMAKE_SOURCE_DIR}/src/nn-768-2x1024-1.nnue")

set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s -O3 -Wall -Wextra -DNDEBUG -funroll-loops -march=native -static -lstdc++ -pthread")

add_definitions(-DNNUE_PATH="${NNUE_FILE_PATH}")

add_executable(Astra        
        src/chess/misc.cpp
        src/chess/attacks.cpp
        src/chess/bitboard.cpp
        src/chess/board.cpp
        src/chess/perft.cpp
        src/chess/zobrist.cpp

        src/search/search.cpp
        src/search/tt.cpp
        src/search/syzygy.cpp
        src/search/moveordering.cpp
        src/search/tune.cpp

        src/eval/nnue.cpp
        src/eval/eval.cpp
      
        src/syzygy/tbprobe.cpp
      
        src/uci.cpp
        src/bench.cpp
        src/main.cpp
)
