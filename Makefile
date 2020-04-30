# Good read on automatic dependencies:
# http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/

CFLAGS += -Wall -Wextra -Wpedantic -Wformat=2 -Wno-unused-parameter -Wshadow
CFLAGS += -Wwrite-strings -Wstrict-prototypes -Wold-style-definition
CFLAGS += -Wredundant-decls -Wnested-externs -Wmissing-include-dirs
CFLAGS += -Wlogical-op -Wjump-misses-init -Wstrict-aliasing -O2 -std=c11 -I.

BUILDDIR  = build
OBJDIR	  = $(BUILDDIR)/obj
DEPDIR    = $(BUILDDIR)/dep
DEPFLAGS  = -MT $@ -MMD -MP -MF $(patsubst %.c,$(DEPDIR)/%.d,$<)
BINS = $(patsubst %,$(BUILDDIR)/%,clox tfuzz tbench lex tdynarray ttag tdis)

$(BINS): | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

all: $(BINS)

$(BUILDDIR)/tfuzz: $(patsubst %,$(OBJDIR)/%.o,test/table_fuzz mem types/dynarray types/list types/str types/table types/tag)
$(BUILDDIR)/tdynarray: $(patsubst %,$(OBJDIR)/%.o,test/dynarray types/dynarray mem)
$(BUILDDIR)/ttag: $(patsubst %,$(OBJDIR)/%.o,test/tag)

# $(BUILDDIR)/clox: $(patsubst %,$(OBJDIR)/%.o,main mem array bytecode compiler lex types val)
# $(BUILDDIR)/tbench: $(patsubst %,$(OBJDIR)/%.o,test/table_bench array mem test/util types val)

%.o: %.c # reset the default rule

$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<

# rules to create dirs
$(BUILDDIR) $(DEPDIR) $(DEPDIR)/test $(DEPDIR)/types $(OBJDIR) $(OBJDIR)/test $(OBJDIR)/types:
	mkdir -p $@

# ensure obj dirs are created
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard *.c)): | $(OBJDIR)
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard test/*.c)): | $(OBJDIR)/test
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard types/*.c)): | $(OBJDIR)/types

DEPFILES := $(patsubst %.c,$(DEPDIR)/%.d, $(wildcard *.c))
TESTDEPFILES := $(patsubst test/%.c,$(DEPDIR)/test/%.d, $(wildcard test/*.c))
TYPESDEPFILES := $(patsubst types/%.c,$(DEPDIR)/types/%.d, $(wildcard types/*.c))

# ensure dep dirs are created and empty rules are present for missing dep files
$(DEPFILES): | $(DEPDIR)
$(TESTDEPFILES): | $(DEPDIR)/test
$(TYPESDEPFILES): $(DEPDIR)/types

# include only dep files that exist
include $(wildcard $(DEPFILES)) 
include $(wildcard $(TESTDEPFILES))
include $(wildcard $(TYPESDEPFILES))
