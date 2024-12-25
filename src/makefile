ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x1024-1.nnue

# Compiler and flags
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
            -flto \
            -march=native \
            -DNNUE_PATH=\"$(EvalFile)\" # nnue path

LDFLAGS := -lstdc++

# Source files
CHESS_SRC := chess/attacks.cpp \
             chess/board.cpp \
             chess/misc.cpp \
             chess/perft.cpp \
             chess/zobrist.cpp

EVAL_SRC := eval/eval.cpp \
            eval/nnue.cpp

SEARCH_SRC := search/history.cpp \
              search/movepicker.cpp \
              search/search.cpp \
              search/syzygy.cpp \
              search/threads.cpp \
              search/tt.cpp \
              search/tune.cpp 
              
SYZYGY_SRC := fathom/tbprobe.cpp

MAIN_SRC := main.cpp \
            bench.cpp \
            uci.cpp

SRC := $(CHESS_SRC) $(EVAL_SRC) $(SEARCH_SRC) $(SYZYGY_SRC) $(MAIN_SRC)

# Default target executable
TARGET = astra

# Detect Operating System
ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -lstdc++ -static
else
    SUFFIX :=
    LDFLAGS := -lpthread -lstdc++
endif

# Detect Platform-specific Cleanup Command
ifeq ($(OS), Windows_NT)
    RM = del /Q
else
    RM = rm -f
endif

TARGET_SUFFIX := $(TARGET)$(SUFFIX)

# Default rule to build executable
all: $(TARGET_SUFFIX)

$(TARGET_SUFFIX): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	$(RM) $(TARGET_SUFFIX)

# PGO (Profile-Guided Optimization) process
pgo:
	$(CXX) $(CXXFLAGS) -fprofile-generate -o $(TARGET_SUFFIX) $(SRC) $(LDFLAGS)
	./$(TARGET_SUFFIX) bench

	$(CXX) $(filter-out -fprofile-generate, $(CXXFLAGS)) -fprofile-use -o $(TARGET_SUFFIX) $(SRC) $(LDFLAGS)

	$(RM) *.gcda *.o profdata

.PHONY: all clean pgo
