cmake_minimum_required(VERSION 3.28)
project(Astra)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -DNDEBUG -funroll-loops -march=native -static -lstdc++ -pthread")

add_executable(Astra
        src/main.cpp
        src/chess/attacks.cpp
        src/chess/board.cpp
        src/search/search.cpp
        src/search/tt.cpp
        src/eval/nnue.cpp
        src/eval/eval.cpp
        src/chess/perft.cpp
        src/syzygy/tbprobe.cpp
        src/uci.cpp
        src/search/moveordering.cpp
)