ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

# Allow custom EVALFILE path via command line
EVALFILE ?= $(ROOT)/weights-1.nnue

# Compiler
CXX := g++

# Compiler flags
CXXFLAGS = -s \
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
           -DNNUE_PATH=\"$(EVALFILE)\"

# Platform-specific settings
ifeq ($(OS),Windows_NT)
    SUFFIX := .exe
    RM := del /Q
    CXXFLAGS += -static
else
    SUFFIX :=
    RM := rm -f
endif

# Source files
CHESS_SRC := chess/bitboard.cpp \
             chess/board.cpp \
             chess/zobrist.cpp \
             chess/cuckoo.cpp

EVAL_SRC := nnue/nnue.cpp \
            nnue/accum.cpp

ENGINE_SRC := search/history.cpp \
              search/movepicker.cpp \
              search/search.cpp \
              search/tt.cpp \
              search/threads.cpp \
              search/tune_params.cpp

SYZYGY_SRC := third_party/fathom/src/tbprobe.c

MAIN_SRC := main.cpp \
            uci/uci.cpp \
            uci/uci_options.cpp

SRC := $(CHESS_SRC) $(EVAL_SRC) $(ENGINE_SRC) $(SYZYGY_SRC) $(MAIN_SRC)

TARGET = $(if $(EXE),$(EXE),astra)
TARGET_SUFFIX := $(TARGET)$(SUFFIX)

all: $(TARGET_SUFFIX)

$(TARGET_SUFFIX): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET_SUFFIX)

.PHONY: all clean
