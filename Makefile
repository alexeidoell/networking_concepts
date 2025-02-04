# Alexei Doell cka067 11345642

CC = gcc
CFLAGS = -g -I. 
CPPFLAGS = -std=gnu99 -Wall -pedantic -Wextra -fPIE

.PHONY: all clean

EXECUTABLES = tcp_server tcp_client tcp_proxy udp_server udp_proxy

all: $(EXECUTABLES)

clean:
	rm -rf build/ $(EXECUTABLES)

OBJ_DIR = build/obj/
LIB_DIR = build/lib/
BIN_DIR = build/bin/

LIB_DIRS = -L$(LIB_DIR)

$(BIN_DIR) $(LIB_DIR) $(OBJ_DIR) :
	mkdir -p $@

$(OBJ_DIR)shared.o : shared.c shared.h | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) shared.c -o $(OBJ_DIR)shared.o

$(OBJ_DIR)tcp_server.o : tcp_server.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) tcp_server.c -o $(OBJ_DIR)tcp_server.o

$(BIN_DIR)tcp_server : $(OBJ_DIR)tcp_server.o $(OBJ_DIR)shared.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)tcp_server.o $(OBJ_DIR)shared.o -o $(BIN_DIR)tcp_server

$(OBJ_DIR)tcp_client.o : tcp_client.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) tcp_client.c -o $(OBJ_DIR)tcp_client.o

$(BIN_DIR)tcp_client : $(OBJ_DIR)tcp_client.o $(OBJ_DIR)shared.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)tcp_client.o $(OBJ_DIR)shared.o -o $(BIN_DIR)tcp_client

$(OBJ_DIR)tcp_proxy.o : tcp_proxy.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) tcp_proxy.c -o $(OBJ_DIR)tcp_proxy.o -lpthread

$(BIN_DIR)tcp_proxy : $(OBJ_DIR)tcp_proxy.o $(OBJ_DIR)shared.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)tcp_proxy.o $(OBJ_DIR)shared.o -o $(BIN_DIR)tcp_proxy -lpthread

$(OBJ_DIR)udp_server.o : udp_server.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) udp_server.c -o $(OBJ_DIR)udp_server.o

$(BIN_DIR)udp_server : $(OBJ_DIR)udp_server.o $(OBJ_DIR)shared.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)udp_server.o $(OBJ_DIR)shared.o -o $(BIN_DIR)udp_server

$(OBJ_DIR)udp_proxy.o : udp_proxy.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) udp_proxy.c -o $(OBJ_DIR)udp_proxy.o -lpthread

$(BIN_DIR)udp_proxy : $(OBJ_DIR)udp_proxy.o $(OBJ_DIR)shared.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)udp_proxy.o $(OBJ_DIR)shared.o -o $(BIN_DIR)udp_proxy

tcp_server : $(BIN_DIR)tcp_server
	ln -sf $(BIN_DIR)tcp_server tcp_server

tcp_client : $(BIN_DIR)tcp_client
	ln -sf $(BIN_DIR)tcp_client tcp_client

tcp_proxy : $(BIN_DIR)tcp_proxy
	ln -sf $(BIN_DIR)tcp_proxy tcp_proxy

udp_proxy : $(BIN_DIR)udp_proxy
	ln -sf $(BIN_DIR)udp_proxy udp_proxy

udp_server : $(BIN_DIR)udp_server
	ln -sf $(BIN_DIR)udp_server udp_server
