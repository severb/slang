INCL	= chunk.h common.h compiler.h debug.h memory.h object.h scanner.h table.h value.h vm.h
CC	= gcc
CFLAGS	= -std=c11 -I. -pedantic -Wall -O2

_OBJ	= chunk.o compiler.o debug.o main.o memory.o object.o scanner.o table.o value.o vm.o
OBJ	= $(patsubst %,build/obj/%,$(_OBJ))


build/obj/%.o: %.c $(INCL)
	$(CC) -c -o $@ $< $(CFLAGS)

build/clox: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

run: build/clox
	build/clox
