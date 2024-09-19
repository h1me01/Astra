cmake_minimum_required(VERSION 3.28)
project(Tsukuyomi)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")

add_executable(Tsukuyomi
        src/main.cpp
        src/chess/types.h
        src/chess/misc.h
        src/chess/bitboard.h
        src/chess/attacks.h
        src/chess/attacks.cpp
        src/chess/board.cpp
        src/chess/board.h
        src/chess/movegen.h
        src/chess/perft.h
        src/search/search.cpp
        src/search/search.h
        src/search/moveordering.cpp
        src/search/moveordering.h
        src/search/tt.h
        src/search/tt.cpp
        src/search/pvTable.h
        src/search/timemanager.h
        src/datagen.h
        src/eval/eval.h
        src/eval/nnue.cpp
        src/eval/nnue.h
        src/eval/eval.cpp
        src/chess/perft.cpp
        src/eval/accumulator.h
        src/datagen.cpp
        src/syzygy/tbprobe.h
        src/syzygy/tbconfig.h
        src/syzygy/tbchess.cpp
        src/syzygy/stdendian.h
        src/syzygy/tbprobe.cpp
        src/uci.cpp
        src/uci.h
        src/options.h

)
