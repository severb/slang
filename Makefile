# Good read on automatic dependencies:
# http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/

CFLAGS += -Wall -Wextra -Wpedantic -Wformat=2 -Wno-unused-parameter -Wshadow
CFLAGS += -Wwrite-strings -Wstrict-prototypes -Wold-style-definition
CFLAGS += -Wredundant-decls -Wnested-externs -Wmissing-include-dirs
CFLAGS += -Wstrict-aliasing -Wmissing-braces -O2 -std=c11 -I.

BUILDDIR  = build
OBJDIR	  = $(BUILDDIR)/obj
DEPDIR    = $(BUILDDIR)/dep
DEPFLAGS  = -MT $@ -MMD -MP -MF $(patsubst %.c,$(DEPDIR)/%.d,$<)
BINS = $(patsubst %,$(BUILDDIR)/%,slang tfuzz tbench tdynarray ttag)

$(BINS): | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

all: $(BINS)

$(BUILDDIR)/slang: $(patsubst %,$(OBJDIR)/%.o,bin/slang mem frontend/bytecode frontend/compiler frontend/lex types/list types/dynarray types/tag types/str types/table)
$(BUILDDIR)/tbench: $(patsubst %,$(OBJDIR)/%.o,test/table_bench mem types/dynarray types/list types/str types/table types/tag)
$(BUILDDIR)/tfuzz: $(patsubst %,$(OBJDIR)/%.o,test/table_fuzz mem types/dynarray types/list types/str types/table types/tag)
$(BUILDDIR)/tdynarray: $(patsubst %,$(OBJDIR)/%.o,test/dynarray types/dynarray mem)
$(BUILDDIR)/ttag: $(patsubst %,$(OBJDIR)/%.o,test/tag)

%.o: %.c # reset the default rule

$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<

# rules to create dirs
$(BUILDDIR) $(DEPDIR) $(DEPDIR)/test $(DEPDIR)/types $(DEPDIR)/frontend $(DEPDIR)/bin $(OBJDIR) $(OBJDIR)/test $(OBJDIR)/types $(OBJDIR)/frontend $(OBJDIR)/bin:
	mkdir -p $@

# ensure obj dirs are created
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard *.c)): | $(OBJDIR)
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard test/*.c)): | $(OBJDIR)/test
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard types/*.c)): | $(OBJDIR)/types
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard frontend/*.c)): | $(OBJDIR)/frontend
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard bin/*.c)): | $(OBJDIR)/bin

DEPFILES := $(patsubst %.c,$(DEPDIR)/%.d, $(wildcard *.c))
TESTDEPFILES := $(patsubst test/%.c,$(DEPDIR)/test/%.d, $(wildcard test/*.c))
TYPESDEPFILES := $(patsubst types/%.c,$(DEPDIR)/types/%.d, $(wildcard types/*.c))
FRONTENDDEPSFILES := $(patsubst frontend/%.c,$(DEPDIR)/frontend/%.d, $(wildcard frontend/*.c))
BINDEPSFILES := $(patsubst bin/%.c,$(DEPDIR)/bin/%.d, $(wildcard bin/*.c))

# ensure dep dirs are created and empty rules are present for missing dep files
$(DEPFILES): | $(DEPDIR)
$(TESTDEPFILES): | $(DEPDIR)/test
$(TYPESDEPFILES): | $(DEPDIR)/types
$(FRONTENDDEPSFILES) : | $(DEPDIR)/frontend
$(BINDEPSFILES) : | $(DEPDIR)/bin

# include only dep files that exist
include $(wildcard $(DEPFILES)) 
include $(wildcard $(TESTDEPFILES))
include $(wildcard $(TYPESDEPFILES))
include $(wildcard $(FRONTENDDEPSFILES))
include $(wildcard $(BINDEPSFILES))
