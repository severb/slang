#include "fun.h"

#include <stdio.h>

#include "mem.h"
#include "str.h"
#include "list.h"

static void fun_destroy(Fun *f) {
    list_destroy(&f->args);
    *f = (Fun){0};
}

void fun_free(Fun *f) {
    fun_destroy(f);
    mem_free(f, sizeof(*f));
}

void fun_printf(FILE *f, const Fun *fun) {
    fputs("<fun: ", f);
    slice_printf(f, &fun->name);
    size_t arg_len = list_len(&fun->args);
    fputc('(', f);
    if (arg_len) {
        for (size_t i = 0; i < arg_len; i++) {
            tag_print(*list_get(&fun->args, i));
            if (i < arg_len - 1) {
                fputs(", ", f);
            }
        }
    }
    fputs(")>", f);
}
