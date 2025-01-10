# Alexei Doell cka067 11345642

CC = gcc
CFLAGS = -g -I.
CPPFLAGS = -std=c11 -Wall -pedantic -Wextra -fPIE

.PHONY: all clean

EXECUTABLES = caesar

all: $(EXECUTABLES)

clean:
	rm -rf build/ $(EXECUTABLES)

OBJ_DIR = build/obj/
LIB_DIR = build/lib/
BIN_DIR = build/bin/

LIB_DIRS = -L$(LIB_DIR)

$(BIN_DIR) $(LIB_DIR) $(OBJ_DIR) :
	mkdir -p $@

$(OBJ_DIR)caesar.o : caesar.c caesar.h | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) caesar.c -o $(OBJ_DIR)caesar.o

$(BIN_DIR)caesar : $(OBJ_DIR)caesar.o | $(BIN_DIR)
	$(CC) $(OBJ_DIR)caesar.o -o $(BIN_DIR)caesar

caesar : $(BIN_DIR)caesar
	ln -sf $(BIN_DIR)caesar caesar

