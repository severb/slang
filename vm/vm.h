#ifndef slang_vm_h
#define slang_vm_h

#include "bytecode.h" // Chunk
#include "table.h"    // Table

#define MAX_FRAMES 1024

typedef struct {
    Fun *f;
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

#endif
