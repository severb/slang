#ifndef slang_fun_h
#define slang_fun_h

#include "list.h" // List
#include "str.h"  // Slice
#include "tag.h"  // Tag

#include <stdio.h> // FILE, size_t

typedef enum { FUN_BUILTIN, FUN_USER } FunType;

typedef struct VM VM;

typedef struct Fun {
    FunType type;
    union {
        struct {
            Slice name;
            Slice signature;
            bool (*fun)(VM *, size_t arity);
        } builtin;
        struct {
            size_t entry; // bytecode entrypoint location
            size_t arity;
            size_t line;
            Tag name;
            List args;
        } user;
    };
} Fun;

void fun_printf(FILE *, const Fun *);
void fun_free(Fun *);
void fun_call(Fun *, size_t n);

#endif
