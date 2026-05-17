CC ?= cc

TARGET := build/vampire-survivors-c
SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2
CFLAGS += -D_POSIX_C_SOURCE=200809L
LDLIBS += -lm

.PHONY: all run clean

all: $(TARGET)

build:
	mkdir -p build

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.c src/game.h | build
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf build
