CC := gcc
CCFLAGS := -Wall -lpthread
LIBS := -lSDL2

SRCS := $(shell find src/ -name "*.c")
TESTSGEN := tests/gentests.c
INCLUDE := include/ -I/usr/include/SDL2 -D_REENTRANT
# change extension of all files from .c to .o
# and prefix from src/ to build/
BUILDS := $(SRCS:src/%.c=build/%.o)
BIN := bin/emu

all: build_dirs $(BIN)

debug: CCFLAGS += -g
debug: all

build_dirs:
	@mkdir -p $(sort $(dir $(BUILDS)))

$(BIN): $(BUILDS)
	@$(CC) $(CCFLAGS) $^ -o $(BIN) $(LIBS)

build/%.o: src/%.c
	@$(CC) $(CCFLAGS) -I$(INCLUDE) -c $< -o $@ $(LIBS)

gentests: $(TESTSGEN)
	@mkdir -p bin/tests/
	@$(CC) -I$(INCLUDE) $^ -o bin/$(TESTSGEN:%.c=%)
	@bin/$(TESTSGEN:%.c=%)

clean:
	@rm -rf build/* bin/*

clean_tests:
	@rm -rf bin/tests/
