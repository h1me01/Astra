
ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

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
            -march=native 
LDFLAGS := -lstdc++

# Source files
CHESS_SRC := chess/bitboard.cpp \
             chess/board.cpp \
             chess/misc.cpp \
             chess/perft.cpp \
             chess/zobrist.cpp 

EVAL_SRC := eval/eval.cpp 

SEARCH_SRC := search/search.cpp \
              search/tt.cpp \
              search/history.cpp 

MAIN_SRC := main.cpp \
            uci.cpp

SRC := $(CHESS_SRC) $(EVAL_SRC) $(SEARCH_SRC) $(MAIN_SRC)
TARGET = $(if $(EXE),$(EXE),astra)

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

.PHONY: all clean pgo