#include "builtins.h"

#include "fun.h"
#include "list.h"
#include "mem.h"
#include "str.h"
#include "tag.h"
#include "vm.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

Tag builtin_print(VM *vm, size_t arity) {
    size_t len = list_len(&vm->stack);
    for (size_t i = len - arity; i < len; i++) {
        Tag t = *list_get(&vm->stack, i);
        tag_print(t);
    }
    putchar('\n');
    return TAG_NIL;
}

Tag builtin_stack_trace(VM *vm, size_t arity) {
    size_t skip = 0;
    if (arity == 1) {
        Tag arg = *list_last(&vm->stack);
        int64_t wanted_skip = 0;
        if (!as_int(arg, &wanted_skip)) {
            Tag *err = mem_allocate(sizeof(*err));
            static Slice msg = SLICE("skip must be an integer");
            *err = tag_to_ref(slice_to_tag(&msg));
            return error_to_tag(err);
        }
        if (wanted_skip < 0) {
            Tag *err = mem_allocate(sizeof(*err));
            static Slice msg = SLICE("skip must be positive");
            *err = tag_to_ref(slice_to_tag(&msg));
            return error_to_tag(err);
        }
        if ((uint64_t)wanted_skip > SIZE_MAX) {
            Tag *err = mem_allocate(sizeof(*err));
            *err = tag_to_ref(slice_to_tag(&SLICE("skip is too large")));
            return error_to_tag(err);
        }
        skip = (uint64_t)wanted_skip;
    }
    for (size_t i = 0; i + skip < vm->current_frame; i++) {
        for (size_t j = 0; j < i; j++) {
            putchar(' ');
            putchar(' ');
        }
        Fun * f = vm->frames[i].f;
        fun_printf(stdout, f);
        if (f->type == FUN_USER) {
          printf(" on line %zu", f->user.line);
        }
        putchar('\n');
    }
    return TAG_NIL;
}

#define BUILTIN(n, f, s)                                                                           \
    (Fun) {                                                                                        \
        .type = FUN_BUILTIN, .builtin = {.name = SLICE(n), .fun = (f), .signature = SLICE(s) }     \
    }

Fun builtins[] = {
    BUILTIN("print", builtin_print, "..."),
    BUILTIN("stack_trace", builtin_stack_trace, "skip=0"),
};

size_t builtins_n = sizeof(builtins) / sizeof(Fun);
