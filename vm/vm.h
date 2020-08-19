#ifndef slang_vm_h
#define slang_vm_h

#include "list.h"  // List
#include "table.h" // Table

#include <stddef.h> // size_t

#define MAX_FRAMES 1024

typedef struct Fun Fun;
typedef struct Chunk Chunk;

typedef struct {
    const Fun *f;
    size_t prev_ip;
    size_t prev_frame_base;
} CallFrame;

typedef struct VM {
    const Chunk *chunk;
    size_t ip;
    List stack;
    size_t frame_base;
    List temps;
    Table globals;
    CallFrame frames[MAX_FRAMES];
    size_t current_frame;
} VM;

bool interpret(const Chunk *);
bool call(VM *, size_t arity);

void runtime_tag(VM *, Tag);
void runtime_err_tag(VM *, const char *, Tag);
void runtime_err(VM *, const char *, const char *detail);

#endif
