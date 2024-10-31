# Root directory
ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x1024-1.nnue

# Compiler and flags
CXX ?= g++
CXXFLAGS = -s -Wall -Wextra -O3 -DNDEBUG -funroll-loops -fno-exceptions -pthread -std=c++17 -march=native
CXXFLAGS += -DNNUE_PATH=\"$(EvalFile)\" # add nnue file path

# Source files
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

# Default target executable (if EXE is not provided)
TARGET = astra

# Detect Operating System
ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -static -lstdc++ -lpthread
else
    SUFFIX :=
    LDFLAGS := -lpthread -lstdc++
endif

# Allow overriding EXE with a custom name
EXE ?= $(TARGET)
EXE := $(EXE)$(SUFFIX)

# Default rule to build the executable
all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	rm -f $(EXE)

.PHONY: all clean
