CXX := g++

ifdef EXE
    TARGET := $(EXE)
else
    TARGET := astra
endif

ifeq ($(OS), Windows_NT)
    TARGET  := $(TARGET).exe
    STATIC  := -static
    RM_RF   := rd /s /q
else
    STATIC  :=
    RM_RF   := rm -rf
endif

DEFAULT_WEIGHTS := weights-v3-perm.nnue
EVALFILE_URL    := https://github.com/h1me01/Astra-Networks/releases/download/weights/$(DEFAULT_WEIGHTS)

ifndef EVALFILE
    EVALFILE       := $(DEFAULT_WEIGHTS)
    DOWNLOAD_NET   := true
endif

CXXFLAGS := -std=c++20 -O3 -march=native -funroll-loops -flto -fno-exceptions \
            -DNDEBUG -pthread $(STATIC) -DNNUE_PATH=\"$(EVALFILE)\"

rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

CXX_SRCS := $(call rwildcard,src/,*.cpp)
C_SRCS   := third_party/fathom/tbprobe.c
ALL_OBJS := $(CXX_SRCS:.cpp=.o) $(C_SRCS:.c=.o)

.PHONY: all pgo download-net clean-objs clean
.DEFAULT_GOAL := all

all: download-net $(TARGET)

download-net:
ifdef DOWNLOAD_NET
	@if [ ! -f "$(DEFAULT_WEIGHTS)" ]; then \
		echo "Downloading $(DEFAULT_WEIGHTS)..." && \
		curl -sL -o "$(DEFAULT_WEIGHTS)" "$(EVALFILE_URL)"; \
	fi
endif

PGO_FLAGS :=

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PGO_FLAGS) -c $< -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) $(PGO_FLAGS) -c $< -o $@

$(TARGET): $(ALL_OBJS)
	$(CXX) $(CXXFLAGS) $(PGO_FLAGS) $(ALL_OBJS) -o $@

pgo:
	$(MAKE) PGO_FLAGS="-fprofile-generate=profdir" $(TARGET)
	./$(TARGET) bench
	$(MAKE) clean-objs
	$(MAKE) PGO_FLAGS="-fprofile-use=profdir -fno-peel-loops -fno-tracer" $(TARGET)
	$(RM_RF) profdir

clean-objs:
	rm -f $(ALL_OBJS)

clean:
	rm -f $(ALL_OBJS) astra astra.exe
	$(RM_RF) profdir
