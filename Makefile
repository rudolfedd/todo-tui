# Makefile — Linux / macOS (ncurses) build.
#
#   make            build ./todo-tui
#   make DEBUG=1    build with sanitizers and debug symbols
#   make install    install to $PREFIX/bin (default /usr/local)
#   make clean

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion
CPPFLAGS += -D_XOPEN_SOURCE=700
LDFLAGS ?=
LDLIBS  += -lncurses

PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
BIN := todo-tui

ifeq ($(DEBUG),1)
CFLAGS  += -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address,undefined
endif

.PHONY: all clean install uninstall test

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

src/main.o:    src/app.h src/editor.h src/model.h src/store.h src/ui.h src/util.h
src/app.o:     src/app.h src/editor.h src/model.h src/store.h src/util.h
src/ui.o:      src/app.h src/curses_compat.h src/editor.h src/model.h src/ui.h src/util.h
src/store.o:   src/json.h src/model.h src/store.h src/util.h
src/json.o:    src/json.h src/util.h
src/model.o:   src/model.h src/util.h
src/editor.o:  src/editor.h src/util.h
src/util.o:    src/util.h

test: tests/test_json tests/test_store
	./tests/test_json
	./tests/test_store

tests/test_json: tests/test_json.c src/json.c src/util.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -Isrc -o $@ $^

tests/test_store: tests/test_store.c src/json.c src/util.c src/model.c src/store.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -Isrc -o $@ $^

install: $(BIN)
	mkdir -p $(DESTDIR)$(BINDIR)
	cp $(BIN) $(DESTDIR)$(BINDIR)/$(BIN)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(BIN)

clean:
	rm -f $(OBJ) $(BIN) tests/test_json tests/test_store
