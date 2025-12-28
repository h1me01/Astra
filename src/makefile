
ROOT := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))

# Default eval file
DEFAULT_WEIGHTS := weights-v2.nnue

# Allow custom EVALFILE path via command line
ifndef EVALFILE
	EVALFILE := $(DEFAULT_WEIGHTS)
	DOWNLOAD_NET := true
endif

# URL for the eval file
EVALFILE_URL := https://github.com/h1me01/Astra-Networks/releases/download/weights/$(DEFAULT_WEIGHTS)

# Compiler
CXX := g++

# Compiler flags
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
			 chess/movegen.cpp \
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

TARGET := $(if $(EXE),$(EXE),astra)

TARGET_SUFFIX := $(TARGET)$(SUFFIX)

all: download-net $(TARGET_SUFFIX)
$(TARGET_SUFFIX): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

download-net:
ifdef DOWNLOAD_NET
ifeq ($(OS),Windows_NT)
	@if exist "$(DEFAULT_WEIGHTS)" ( \
		echo Using $(DEFAULT_WEIGHTS) as network file. \
	) else ( \
		echo Downloading $(DEFAULT_WEIGHTS)... && \
		curl -sL -o "$(DEFAULT_WEIGHTS)" "$(EVALFILE_URL)" \
	)
else
	@if test -f "$(DEFAULT_WEIGHTS)"; then \
		echo "Using $(DEFAULT_WEIGHTS) as network file."; \
	else \
		echo "Downloading $(DEFAULT_WEIGHTS)..."; \
		curl -sL -o "$(DEFAULT_WEIGHTS)" "$(EVALFILE_URL)"; \
	fi
endif
else
	@echo "Using network file: $(EVALFILE)"
endif

clean:
	$(RM) $(TARGET_SUFFIX)

.PHONY: all download-net clean
