# Define the compiler and flags
CXX ?= g++
CXXFLAGS = -Wall -Wextra -O3 -DNDEBUG -funroll-loops -pthread -std=c++17 -march=native

# Define the source files
SRC = main.cpp \
      chess/attacks.cpp \
      chess/board.cpp \
      chess/perft.cpp \
      eval/nnue.cpp \
      eval/eval.cpp \
      search/search.cpp \
      search/moveordering.cpp \
      search/tt.cpp \
      datagen.cpp \
      syzygy/tbchess.cpp \
      syzygy/tbprobe.cpp \
      uci.cpp

# Define the target executable
TARGET = astra

# Detect Operating System
ifeq ($(OS), Windows_NT)
    SUFFIX := .exe
    LDFLAGS := -static -lstdc++ -lpthread
else
    SUFFIX :=
    LDFLAGS := -lpthread
endif

# Complete target name with suffix
EXE := $(TARGET)$(SUFFIX)

# Default rule to build the executable
all: $(EXE)

$(EXE): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Clean rule to remove the executable
clean:
	$(RM) $(EXE)

.PHONY: all clean
