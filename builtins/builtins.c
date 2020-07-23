#include "builtins.h"

#include "fun.h"
#include "mem.h"
#include "str.h"
#include "tag.h"

Tag builtin_print(List *stack, size_t arity) {
    size_t len = list_len(stack);
    for (size_t i = len - arity; i < len; i++) {
        Tag t = *list_get(stack, i);
        tag_print(t);
    }
    putchar('\n');
    return TAG_NIL;
}

#define BUILTIN(n, f, s)                                                                           \
    (Fun) {                                                                                        \
        .type = FUN_BUILTIN, .builtin = {.name = SLICE(n), .fun = (f), .signature = SLICE(s) }     \
    }

Fun builtins[] = {
    BUILTIN("print", builtin_print, "..."),
};

size_t builtins_n = sizeof(builtins) / sizeof(Fun);
