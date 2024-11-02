ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x1024-1.nnue

# compiler and flags
CXX := g++
CXXFLAGS := -s -Wall -Wextra -Wcast-qual -O3 -DNDEBUG -funroll-loops -fno-exceptions -pedantic -pthread -std=c++17 -march=native
CXXFLAGS += -DNNUE_PATH=\"$(EvalFile)\" # add nnue file path

LDFLAGS := -pthread -lstdc++ -static 

# source files
SRC = chess/attacks.cpp \
      chess/board.cpp \
      chess/perft.cpp \
      chess/bitboard.cpp \
      chess/misc.cpp \
      chess/zobrist.cpp \
      eval/nnue.cpp \
      eval/eval.cpp \
      search/search.cpp \
      search/syzygy.cpp \
      search/movepicker.cpp \
      search/tt.cpp \
      search/tune.cpp \
      search/history.cpp \
      syzygy/tbprobe.cpp \
      main.cpp \
      bench.cpp \
      uci.cpp

# default target executable
TARGET = astra

# Detect Operating System
ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
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
