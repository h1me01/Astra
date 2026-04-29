
DEFAULT_WEIGHTS := selfgen_v18.nnue

# Allow custom EVALFILE path via command line
ifndef EVALFILE
	EVALFILE := $(DEFAULT_WEIGHTS)
	DOWNLOAD_NET := true
endif

EVALFILE_URL := https://github.com/h1me01/Astra-Networks/releases/download/weights/$(DEFAULT_WEIGHTS)

CXX := g++

CXXFLAGS := -s \
			-std=c++20 \
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

ifeq ($(OS),Windows_NT)
	SUFFIX := .exe
	RM := del /Q
	CXXFLAGS += -static
else
	SUFFIX :=
	RM := rm -f
endif

rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
SRC := $(call rwildcard,./src,*.cpp) src/third_party/fathom/src/tbprobe.c

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
