# Alexei Doell cka067 11345642

CC = gcc
CFLAGS = -g -I. 
CPPFLAGS = -std=gnu99 -Wall -pedantic -Wextra -fPIE

.PHONY: all clean

LISTLIB_SRC = list_adders.c list_movers.c list_removers.c list_alloc.c
EXECUTABLES = receiver sender

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

$(OBJ_DIR)receiver.o : receiver.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) receiver.c -o $(OBJ_DIR)receiver.o

$(BIN_DIR)receiver : $(OBJ_DIR)receiver.o | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)receiver.o -o $(BIN_DIR)receiver

$(OBJ_DIR)sender.o : sender.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) sender.c -o $(OBJ_DIR)sender.o

$(BIN_DIR)sender : $(OBJ_DIR)sender.o $(LIB_DIR)liblist.a | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LIB_DIRS) $(OBJ_DIR)sender.o -llist -o $(BIN_DIR)sender

sender : $(BIN_DIR)sender
	ln -sf $(BIN_DIR)sender sender

receiver : $(BIN_DIR)receiver
	ln -sf $(BIN_DIR)receiver receiver
