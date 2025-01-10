# Alexei Doell cka067 11345642

CC = gcc
CFLAGS = -g -I.
CPPFLAGS = -std=gnu11 -Wall -pedantic -Wextra -fPIE

.PHONY: all clean

EXECUTABLES = tcp_server tcp_client

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

$(OBJ_DIR)tcp_server.o : tcp_server.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) tcp_server.c -o $(OBJ_DIR)tcp_server.o

$(BIN_DIR)tcp_server : $(OBJ_DIR)tcp_server.o $(OBJ_DIR)caesar.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)tcp_server.o $(OBJ_DIR)caesar.o -o $(BIN_DIR)tcp_server

$(OBJ_DIR)tcp_client.o : tcp_client.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) tcp_client.c -o $(OBJ_DIR)tcp_client.o

$(BIN_DIR)tcp_client : $(OBJ_DIR)tcp_client.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)tcp_client.o -o $(BIN_DIR)tcp_client

tcp_server : $(BIN_DIR)tcp_server
	ln -sf $(BIN_DIR)tcp_server tcp_server

tcp_client : $(BIN_DIR)tcp_client
	ln -sf $(BIN_DIR)tcp_client tcp_client
