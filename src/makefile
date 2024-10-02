# Define compiler and flags
CXX ?= g++
CXXFLAGS = -Wall -Wextra -O3 -DNDEBUG -funroll-loops -pthread -static -std=c++17 -march=native

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

# Define the header files (not directly used for compilation)
HDR = chess/types.h \
      chess/misc.h \
      chess/bitboard.h \
      chess/attacks.h \
      chess/board.h \
      chess/movegen.h \
      chess/perft.h \
      search/search.h \
      search/moveordering.h \
      search/pvTable.h \
      search/timemanager.h \
      datagen.h \
      eval/eval.h \
      eval/nnue.h \
      eval/accumulator.h \
      syzygy/tbconfig.h \
      syzygy/stdendian.h \
      syzygy/tbprobe.h \
      options.h

# Define the target executable
TARGET = astra

# Default rule to build the executable
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Clean rule to remove the executable
clean:
	$(RM) $(TARGET)

.PHONY: all clean
