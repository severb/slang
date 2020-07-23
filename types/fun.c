#include "fun.h"

#include <stdio.h>

#include "list.h"
#include "mem.h"
#include "str.h"
#include "types/tag.h"

static void fun_destroy(Fun *fun) {
    switch (fun->type) {
    case (FUN_BUILTIN):
        break;
    case (FUN_USER):
        tag_free(fun->user.name);
        list_destroy(&fun->user.args);
    }
    *fun = (Fun){0};
}

void fun_free(Fun *fun) {
    fun_destroy(fun);
    mem_free(fun, sizeof(*fun));
}

void fun_printf(FILE *f, const Fun *fun) {
    putc('<', f);
    switch (fun->type) {
    case (FUN_BUILTIN):
        fputs("fun builtin: ", f);
        slice_printf(f, &fun->builtin.name);
        putc('(', f);
        slice_printf(f, &fun->builtin.signature);
        putc(')', f);
        break;
    case (FUN_USER):
        fputs("fun: ", f);
        tag_print(fun->user.name);
        putc('(', f);
        const List *args = &fun->user.args;
        size_t arg_len = list_len(args);
        if (arg_len) {
            for (size_t i = 0; i < arg_len; i++) {
                tag_print(*list_get(args, i));
                if (i < arg_len - 1) {
                    fputs(", ", f);
                }
            }
        }
        putc(')', f);
        break;
    }
    putc('>', f);
}
