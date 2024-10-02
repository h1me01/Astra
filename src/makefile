# compiler and flags
CXX ?= g++
CXXFLAGS = -Wall -Wextra -O3 -DNDEBUG -funroll-loops -pthread -std=c++17 -march=native

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
      datagen.cpp \
      syzygy/tbchess.cpp \
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

# Clean rule to remove the executable
clean:
	$(RM) $(EXE)

.PHONY: all clean
