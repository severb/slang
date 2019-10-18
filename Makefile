CFLAGS += -std=c11 -I. -pedantic -Wall -O3

BUILDDIR = build
OBJDIR	 = $(BUILDDIR)/obj

$(OBJDIR)/%.o: %.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/bench/%.o: bench/%.c common.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/clox: $(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c))
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/bench_table: $(patsubst %,$(OBJDIR)/%,mem.o str.o table.o bench/table.o val.o)
	$(CC) $(CFLAGS) -o $@ $^


$(OBJDIR)/chunk.o:       array.h chunk.h mem.h str.h table.h val.h
$(OBJDIR)/compiler.o:    array.h chunk.h intern.h mem.h scanner.h str.h table.h val.h
$(OBJDIR)/intern.o:      intern.h str.h val.h
$(OBJDIR)/main.o:        chunk.h compiler.h intern.h str.h table.h val.h
$(OBJDIR)/mem.o:         mem.h
$(OBJDIR)/scanner.o:     scanner.h
$(OBJDIR)/str.o:         mem.h str.h
$(OBJDIR)/table.o:       mem.h table.h val.h
$(OBJDIR)/val.o:         mem.h str.h table.h val.h
$(OBJDIR)/vm.o:          array.h chunk.h compiler.h intern.h str.h table.h val.h vm.h

$(OBJDIR)/bench/table.o: mem.h str.h table.h val.h

# ensure bulid directories exist
$(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c)): | $(OBJDIR)
$(patsubst %.c,$(OBJDIR)/%.o,$(wildcard bench/*.c)): | $(OBJDIR)/bench

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/bench:
	mkdir -p $(OBJDIR)/bench

.PHONY: clean bench
clean:
	rm -f $(BUILDDIR)/clox
	rm -f $(BUILDDIR)/bench_table
	rm -f $(patsubst %.c,$(OBJDIR)/%.o,$(wildcard *.c))
	rm -f $(patsubst %.c,$(OBJDIR)/%.o,$(wildcard bench/*.c))

bench: $(BUILDDIR)/bench_table
