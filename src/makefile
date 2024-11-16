ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x1024-1.nnue
AVX2 := -mavx2 

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
            -DNNUE_PATH=\"$(EvalFile)\" # nnue path

LDFLAGS := -lstdc++

# Source files
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

# Default target executable
TARGET_NATIVE = astra-x86-64
TARGET_AVX2 = astra-avx2-x86-64

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

TARGET_NATIVE_SUFFIX := $(TARGET_NATIVE)$(SUFFIX)
TARGET_AVX2_SUFFIX := $(TARGET_AVX2)$(SUFFIX)

CXX_FLAGS_NATIVE := $(CXXFLAGS) -march=native
CXX_FLAGS_AVX2 := $(CXXFLAGS) $(AVX2) -DAVX2

# Default rule to build executable
all: $(TARGET_NATIVE) $(TARGET_AVX2)

$(TARGET_NATIVE):
$(TARGET_NATIVE): $(SRC)
	$(CXX) $(CXX_FLAGS_NATIVE) -o $@ $^ $(LDFLAGS)

$(TARGET_AVX2): 
$(TARGET_AVX2): $(SRC)
	$(CXX) $(CXX_FLAGS_AVX2) -o $@ $^ $(LDFLAGS)

# Clean target
clean:
	$(RM) $(TARGET_NATIVE) $(TARGET_AVX2)

# PGO (Profile-Guided Optimization) process
pgo:
	$(CXX) $(CXX_FLAGS_NATIVE) -fprofile-generate -o $(TARGET_NATIVE) $(SRC) $(LDFLAGS)
	./$(TARGET_NATIVE) bench

	$(CXX) $(CXX_FLAGS_AVX2) -fprofile-generate -o $(TARGET_AVX2) $(SRC) $(LDFLAGS)
	./$(TARGET_AVX2) bench

	$(CXX) $(filter-out -fprofile-generate, $(CXX_FLAGS_NATIVE)) -fprofile-use -o $(TARGET_NATIVE) $(SRC) $(LDFLAGS)
	$(CXX) $(filter-out -fprofile-generate, $(CXX_FLAGS_AVX2)) -fprofile-use -o $(TARGET_AVX2) $(SRC) $(LDFLAGS)

	$(RM) *.gcda *.o $(DEPENDS) profdata

.PHONY: all clean pgo
