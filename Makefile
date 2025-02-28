# Alexei Doell cka067 11345642

CC = gcc
CFLAGS = -g -I. 
CPPFLAGS = -std=gnu11 -Wall -Wextra -fPIE

.PHONY: all clean

LISTLIB_SRC = list_adders.c list_movers.c list_removers.c list_alloc.c
EXECUTABLES = router

all: $(EXECUTABLES)

clean:
	rm -rf build/ $(EXECUTABLES)

OBJ_DIR = build/obj/
LIB_DIR = build/lib/
BIN_DIR = build/bin/

LIB_DIRS = -L$(LIB_DIR) -L.

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

$(OBJ_DIR)router.o : router.c | $(OBJ_DIR)
	$(CC) -c $(CPPFLAGS) $(CFLAGS) router.c -o $(OBJ_DIR)router.o

$(BIN_DIR)router : $(OBJ_DIR)router.o $(LIB_DIR)liblist.a | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(OBJ_DIR)router.o $(LIB_DIRS) -luring -llist -o $(BIN_DIR)router

router : $(BIN_DIR)router
	ln -sf $(BIN_DIR)router router
