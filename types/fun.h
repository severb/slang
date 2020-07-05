#ifndef slang_fun_h
#define slang_fun_h

// #include "bytecode.h"
#include "str.h"
#include "list.h"

typedef struct Fun {
    size_t entry; // bytecode entrypoint location
    size_t arity; // arity cache
    Slice name;
    size_t line;
    List args;
} Fun;

void fun_printf(FILE *, const Fun *);
void fun_free(Fun *);
void fun_call(Fun *, size_t n);

#endif
