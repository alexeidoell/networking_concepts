# Alexei Doell cka067 11345642

CC = gcc
CFLAGS = -g -I. 
CPPFLAGS = -std=gnu99 -Wall -pedantic -Wextra -fPIE

.PHONY: all clean

LISTLIB_SRC = list_adders.c list_movers.c list_removers.c list_alloc.c
EXECUTABLES = udp_server udp_proxy

all: $(EXECUTABLES)

clean:
	rm -rf build/ $(EXECUTABLES)

OBJ_DIR = build/obj/
LIB_DIR = build/lib/
BIN_DIR = build/bin/

LIB_DIRS = -L$(LIB_DIR)

$(BIN_DIR) $(LIB_DIR) $(OBJ_DIR) :
	mkdir -p $@

$(OBJ_DIR)list_adders.o : list_adders.c list.h | $(OBJECT_DIR) 
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(INCLUDES) list_adders.c -o \
		$(OBJ_DIR)list_adders.o

$(OBJ_DIR)list_movers.o : list_movers.c list.h | $(OBJECT_DIR) 
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(INCLUDES) list_movers.c -o \
		$(OBJ_DIR)list_movers.o

$(OBJ_DIR)list_removers.o : list_removers.c list.h | $(OBJECT_DIR) 
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(INCLUDES) list_removers.c -o \
		$(OBJ_DIR)list_removers.o

$(OBJ_DIR)list_alloc.o : list_alloc.c list.h | $(OBJECT_DIR) 
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(INCLUDES) list_alloc.c -o \
		$(OBJ_DIR)list_alloc.o

$(LIB_DIR)liblist.a : $(OBJ_DIR)list_adders.o $(OBJ_DIR)list_movers.o \
	$(OBJ_DIR)list_removers.o $(OBJ_DIR)list_alloc.o list.h \
	list_alloc.h | $(LIB_DIR)
	ar rcs $(LIB_DIR)liblist.a $(OBJ_DIR)list_adders.o \
	$(OBJ_DIR)list_movers.o $(OBJ_DIR)list_removers.o \
	$(OBJ_DIR)list_alloc.o

$(OBJ_DIR)shared.o : shared.c shared.h | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) shared.c -o $(OBJ_DIR)shared.o

$(OBJ_DIR)udp_server.o : udp_server.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) udp_server.c -o $(OBJ_DIR)udp_server.o

$(BIN_DIR)udp_server : $(OBJ_DIR)udp_server.o $(OBJ_DIR)shared.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)udp_server.o $(OBJ_DIR)shared.o -o $(BIN_DIR)udp_server

$(OBJ_DIR)udp_proxy.o : udp_proxy.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) udp_proxy.c -o $(OBJ_DIR)udp_proxy.o -lpthread

$(BIN_DIR)udp_proxy : $(OBJ_DIR)udp_proxy.o $(OBJ_DIR)shared.o $(LIB_DIR)liblist.a | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)udp_proxy.o $(OBJ_DIR)shared.o -o $(BIN_DIR)udp_proxy

udp_proxy : $(BIN_DIR)udp_proxy
	ln -sf $(BIN_DIR)udp_proxy udp_proxy

udp_server : $(BIN_DIR)udp_server
	ln -sf $(BIN_DIR)udp_server udp_server
