ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/weights.net

# Compiler and flags
CXX := g++
CXXFLAGS := -g \
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
             chess/zobrist.cpp \
             chess/cuckoo.cpp

EVAL_SRC := net/nnue.cpp \
            net/accumulator.cpp

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

TARGET = astra

ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -lstdc++ -static
else
    SUFFIX :=
    LDFLAGS := -lpthread -lstdc++
endif

ifeq ($(OS), Windows_NT)
    RM = del /Q
else
    RM = rm -f
endif

TARGET_SUFFIX := $(TARGET)$(SUFFIX)

all: $(TARGET_SUFFIX)

$(TARGET_SUFFIX): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET_SUFFIX)

# PGO (Profile-Guided Optimization) process
pgo:
	$(CXX) $(CXXFLAGS) -fprofile-generate -o $(TARGET_SUFFIX) $(SRC) $(LDFLAGS)
	./$(TARGET_SUFFIX) bench

	$(CXX) $(filter-out -fprofile-generate, $(CXXFLAGS)) -fprofile-use -o $(TARGET_SUFFIX) $(SRC) $(LDFLAGS)

	$(RM) *.gcda *.o profdata

# Run with Valgrind to detect memory issues
valgrind:
	$(CXX) $(CXXFLAGS) -o $(TARGET_SUFFIX) $(SRC) $(LDFLAGS)
	valgrind --track-origins=yes --leak-check=full --error-exitcode=1 ./$(TARGET_SUFFIX) bench

.PHONY: all clean pgo valgrind
