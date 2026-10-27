# vividlog-c Makefile
# 第 1 步只需要 sqlite3；curl/git2 在第 2、3 步会用到，先链上无妨
CC      ?= gcc
CFLAGS  ?= -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2 -Iinclude
LDLIBS  := -lsqlite3 -lcurl -lgit2

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)
BIN := vividlog-c

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build $(BIN)

# 内存检查（查泄漏/越界，Java 程序员的新朋友）
check: $(BIN)
	valgrind --leak-check=full --error-exitcode=1 ./$(BIN) help

.PHONY: all clean check
