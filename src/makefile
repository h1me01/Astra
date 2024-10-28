# variables
ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x1024-1.nnue

# compiler and flags
CXX ?= g++
CXXFLAGS = -s -Wall -Wextra -O3 -DNDEBUG -funroll-loops -fno-exceptions -pthread -std=c++17 -march=native
CXXFLAGS += -DNNUE_PATH=\"$(EvalFile)\" # add nnue file path

# source files
SRC = main.cpp \
      chess/attacks.cpp \
      chess/board.cpp \
      chess/perft.cpp \
      chess/bitboard.cpp \
      chess/misc.cpp \
      chess/zobrist.cpp \
      eval/nnue.cpp \
      eval/eval.cpp \
      search/search.cpp \
      search/syzygy.cpp \
      search/moveordering.cpp \
      search/tt.cpp \
      search/tune.cpp \
      syzygy/tbprobe.cpp \
      bench.cpp \
      uci.cpp

# target executable
TARGET = astra

# detect Operating System
ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -static -lstdc++ -lpthread
else
    SUFFIX :=
    LDFLAGS := -lpthread -lstdc++
endif

# complete target name with suffix
EXE := $(TARGET)$(SUFFIX)

# default rule to build the executable
all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# clean target
clean:
	rm -f $(EXE)

.PHONY: all clean
