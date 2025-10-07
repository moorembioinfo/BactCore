# -------- BactCore Makefile (full) --------

# Compiler and binary name
CC      ?= gcc
BIN     ?= BactCore

# Build toggles
# - PORTABLE=1 turns OFF SIMD/LTO/zlib autodetects for maximum compatibility
PORTABLE ?= 0
OMP      ?= 0                                # threading removed; keep 0
LTO      ?= $(if $(filter 1,$(PORTABLE)),0,1)
SIMD     ?= $(if $(filter 1,$(PORTABLE)),0,1)
AVX512   ?= 0
ZLIB     ?= $(if $(filter 1,$(PORTABLE)),0,1)

# Base flags
CFLAGS  ?= -O3 -Wall -Wextra -std=c11 -DNDEBUG -MMD -MP
LDFLAGS ?=

# OpenMP (kept configurable; default off)
ifeq ($(OMP),1)
  CFLAGS  += -fopenmp
  LDFLAGS += -fopenmp
endif

# Link Time Optimization
ifeq ($(LTO),1)
  CFLAGS  += -flto
  LDFLAGS += -flto
endif

# SIMD (AVX2 baseline; optional AVX-512)
ifeq ($(SIMD),1)
  CFLAGS += -mavx2 -mbmi2 -DSIMD_ENABLED
  ifeq ($(AVX512),1)
    CFLAGS += -mavx512f -mavx512bw -mavx512vl -mavx512dq
  endif
endif

# zlib / zlib-ng (optional; autodetect via pkg-config)
ifeq ($(ZLIB),1)
  ifeq ($(shell pkg-config --exists zlib-ng && echo yes),yes)
    CFLAGS  += $(shell pkg-config --cflags zlib-ng) -DUSE_ZLIB
    LDFLAGS += $(shell pkg-config --libs zlib-ng)
  else ifeq ($(shell pkg-config --exists zlib && echo yes),yes)
    CFLAGS  += $(shell pkg-config --cflags zlib) -DUSE_ZLIB
    LDFLAGS += $(shell pkg-config --libs zlib)
  endif
endif

# Sources: compile everything under src/
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
DEP := $(OBJ:.o=.d)

# Default target
all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# Pattern rule
src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Convenience targets
.PHONY: clean native portable pgo-gen pgo-use
clean:
	rm -f $(OBJ) $(DEP) $(BIN) *.gcda *.gcno *.profraw *.profdata

# CPU-tuned build: adds native ISA opts on top of current toggles
native: CFLAGS += -march=native -mtune=native -fno-math-errno -fno-trapping-math -frename-registers -funroll-loops
native: all

# Conservative build (turns off SIMD/LTO/ZLIB autodetects)
portable:
	$(MAKE) PORTABLE=1 clean all

# Profile-guided optimization helpers
#pgo-gen: CFLAGS += -fprofile-generate
#pgo-gen: clean all

#pgo-use: CFLAGS += -fprofile-use
#pgo-use: clean all

# Profile-guided optimization helpers (GCC/Clang)
pgo-gen: CFLAGS  += -fprofile-generate
pgo-gen: LDFLAGS += -fprofile-generate
pgo-gen: clean all

pgo-use: CFLAGS  += -fprofile-use
pgo-use: LDFLAGS += -fprofile-use
pgo-use: clean all



# Include dependency files
-include $(DEP)
# -------- end Makefile --------
