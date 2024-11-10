ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x1024-1.nnue

# compiler and flags
CXX := g++
CXXFLAGS := -s \
            -std=c++17 \
            -Wall \
            -Wextra \
            -Wcast-qual \
            -O3 \
            -DNDEBUG \
            -funroll-loops \
            -pthread \
            -fno-exceptions \
            -march=native \
            -flto \
            -DNNUE_PATH=\"$(EvalFile)\" #nnue path

LDFLAGS := -lstdc++  

# source files
CHESS_SRC := chess/attacks.cpp \
             chess/board.cpp \
             chess/perft.cpp \
             chess/bitboard.cpp \
             chess/misc.cpp \
             chess/zobrist.cpp

EVAL_SRC := eval/nnue.cpp \
            eval/eval.cpp

SEARCH_SRC := search/search.cpp \
              search/syzygy.cpp \
              search/movepicker.cpp \
              search/tt.cpp \
              search/tune.cpp \
              search/history.cpp \
              search/threads.cpp

SYZYGY_SRC := fathom/tbprobe.cpp

MAIN_SRC := main.cpp \
            bench.cpp \
            uci.cpp

SRC := $(CHESS_SRC) $(EVAL_SRC) $(SEARCH_SRC) $(SYZYGY_SRC) $(MAIN_SRC)

# default target executable
TARGET = astra

# Detect Operating System
ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -lstdc++ -static
else
    SUFFIX :=
    LDFLAGS := -lpthread -lstdc++
endif

# allow overriding exe name (used by openbench)
EXE ?= $(TARGET)
EXE := $(EXE)$(SUFFIX)

# default rule to build exe
all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# clean target
clean:
	rm -f $(EXE)

.PHONY: all clean