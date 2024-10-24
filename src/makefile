# variables
ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
EvalFile = $(ROOT)/nn-768-2x512-1.nnue

# compiler and flags
CXX ?= g++
CXXFLAGS = -Wall -Wextra -O3 -DNDEBUG -funroll-loops -pthread -std=c++17 -march=native
CXXFLAGS += -DNNUE_PATH=\"$(EvalFile)\" # add nnue file path

# source files
SRC = main.cpp \
      chess/attacks.cpp \
      chess/board.cpp \
      chess/perft.cpp \
      eval/nnue.cpp \
      eval/eval.cpp \
      search/search.cpp \
      search/moveordering.cpp \
      search/tt.cpp \
      syzygy/tbprobe.cpp \
      uci.cpp

# target executable
TARGET = astra

# detect Operating System
ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -static -lstdc++ -lpthread
else
    SUFFIX :=
    LDFLAGS := -lpthread
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
