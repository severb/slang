CFLAGS	=-std=c11 -I. -pedantic -Wall -O2

BIN	=clox
BUILDDIR=build
OBJDIR	=$(BUILDDIR)/obj
OBJS	=$(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c))

$(BUILDDIR)/$(BIN): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: %.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/chunk.o:    chunk.h memory.h
$(OBJDIR)/compiler.o: chunk.h memory.h object.h scanner.h table.h
$(OBJDIR)/debug.o:    chunk.h debug.h scanner.h value.h
$(OBJDIR)/main.o:     table.h vm.h
$(OBJDIR)/memory.o:   memory.h
$(OBJDIR)/object.o:   memory.h object.h
$(OBJDIR)/scanner.o:  scanner.h
$(OBJDIR)/table.o:    memory.h object.h table.h value.h
$(OBJDIR)/value.o:    memory.h object.h value.h
$(OBJDIR)/vm.o:       chunk.h compiler.h debug.h memory.h object.h table.h value.h vm.h

.PHONY: clean

clean:
	rm $(OBJS)
	rm $(BUILDDIR)/$(BIN)
