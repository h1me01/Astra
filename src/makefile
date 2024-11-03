ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x1024-1.nnue

# Set release as the default target
release: CXXFLAGS += -s
release: clean all

# default target
TARGET = astra

# compiler and flags
CXX := g++
CXXFLAGS := -std=c++17 \
            -Wall \
            -Wextra \
            -Wcast-qual \
            -O3 \
            -DNDEBUG \
            -funroll-loops \
            -fno-exceptions \
            -pedantic \
            -pthread \
            -march=native \
            -DNNUE_PATH=\"$(EvalFile)\" #nnue path

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
              search/history.cpp

SYZYGY_SRC := syzygy/tbprobe.cpp

MAIN_SRC := main.cpp \
            bench.cpp \
            uci.cpp

SRC := $(CHESS_SRC) $(EVAL_SRC) $(SEARCH_SRC) $(SYZYGY_SRC) $(MAIN_SRC)

# detect operating system and set defaults
ifeq ($(OS),Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -pthread -lstdc++ -static
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
ifeq ($(OS),Windows_NT)
	del /f $(EXE)
else
	rm -f $(EXE)
endif

# debug and release targets
debug: CXXFLAGS += -g -DDEBUG
debug: clean all

.PHONY: all clean release debug
