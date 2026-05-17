CC ?= cc

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2

ifeq ($(OS),Windows_NT)
TARGET := build/vampire-survivors-c.exe
CFLAGS += -D_CRT_SECURE_NO_WARNINGS
else
TARGET := build/vampire-survivors-c
CFLAGS += -D_POSIX_C_SOURCE=200809L
endif

LDLIBS += -lm

.PHONY: all run clean

all: $(TARGET)

build:
	mkdir -p build

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ -o $@ $(LDLIBS)

build/%.o: src/%.c src/game.h src/platform.h | build
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET)

clean:
	rm -rf build
