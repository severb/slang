# Good read on automatic dependencies:
# http://make.mad-scientist.net/papers/advanced-auto-dependency-generation/

CFLAGS += -std=c11 -I. -pedantic -Wall -O3

BUILDDIR  = build
OBJDIR	  = $(BUILDDIR)/obj
DEPDIR    = $(BUILDDIR)/dep
DEPFLAGS  = -MT $@ -MMD -MP -MF $(patsubst %.c,$(DEPDIR)/%.d,$<)
BINS = $(patsubst %,$(BUILDDIR)/%,clox tfuzz tbench vtest lex array)

$(BINS): | $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

all: $(BINS)

$(BUILDDIR)/clox: $(patsubst %,$(OBJDIR)/%.o,main mem types val)
$(BUILDDIR)/tfuzz: $(patsubst %,$(OBJDIR)/%.o,test/table_fuzz test/util mem types val)
$(BUILDDIR)/tbench: $(patsubst %,$(OBJDIR)/%.o,test/table_bench test/util mem types val)
$(BUILDDIR)/vtest: $(patsubst %,$(OBJDIR)/%.o,test/val_test mem types val)
$(BUILDDIR)/lex: $(patsubst %,$(OBJDIR)/%.o,test/lex lex)
$(BUILDDIR)/array: $(patsubst %,$(OBJDIR)/%.o,test/array array mem)

%.o: %.c # reset the default rule

$(OBJDIR)/%.o: %.c $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<

# rules to create dirs
$(BUILDDIR) $(DEPDIR) $(DEPDIR)/test $(OBJDIR) $(OBJDIR)/test:
	mkdir -p $@

# ensure obj dirs are created
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard *.c)): | $(OBJDIR)
$(patsubst %.c,$(OBJDIR)/%.o, $(wildcard test/*.c)): | $(OBJDIR)/test

DEPFILES := $(patsubst %.c,$(DEPDIR)/%.d, $(wildcard *.c))
TESTDEPFILES := $(patsubst test/%.c,$(DEPDIR)/test/%.d, $(wildcard test/*.c))

# ensure dep dirs are created and empty rules are present for missing dep files
$(DEPFILES): | $(DEPDIR)
$(TESTDEPFILES): | $(DEPDIR)/test

# include only dep files that exist
include $(wildcard $(DEPFILES)) 
include $(wildcard $(TESTDEPFILES))
