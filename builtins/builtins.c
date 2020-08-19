#include "builtins.h"

#include "fun.h"
#include "list.h"
#include "mem.h"
#include "str.h"
#include "tag.h"
#include "vm.h" // run, call

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool builtin_print(VM *vm, size_t arity) {
    size_t len = list_len(&vm->stack);
    for (size_t i = len - arity; i < len; i++) {
        Tag t = *list_get(&vm->stack, i);
        tag_print(t);
    }
    putchar('\n');
    *list_last(&vm->stack) = TAG_NIL;
    return true;
}

static bool to_int(size_t len, Tag *t) {
    if (len < I49_MAX) {
        *t = i49_to_tag(len);
        return true;
    } else if (len < INT64_MAX) {
        int64_t *i = mem_allocate(sizeof(*i));
        *i = len;
        *t = i64_to_tag(i);
        return true;
    }
    return false;
}

bool builtin_len(VM *vm, size_t arity) {
    if (arity != 1) {
        runtime_err(vm, "len takes exactly one argument", 0);
        return false;
    }
    Tag t = *list_last(&vm->stack);
    switch (tag_type(t)) {
    case TYPE_LIST: {
        if (to_int(list_len(tag_to_list(t)), list_last(&vm->stack))) {
            return true;
        }
        runtime_err_tag(vm, "list size too large: ", t);
        return false;
    }
    case TYPE_TABLE:
        if (to_int(table_len(tag_to_table(t)), list_last(&vm->stack))) {
            return true;
        }
        runtime_err_tag(vm, "table size too large: ", t);
        return true;
    case TYPE_SLICE:
        if (to_int(tag_to_slice(t)->len, list_last(&vm->stack))) {
            return true;
        }
        runtime_err_tag(vm, "string size too large: ", t);
        return false;
    case TYPE_STRING:
        if (to_int(tag_to_string(t)->len, list_last(&vm->stack))) {
            return true;
        }
        runtime_err_tag(vm, "string size too large: ", t);
        return false;
    default: {
        runtime_err_tag(vm, "cannot len value: ", t);
        return false;
    }
    }
}

bool builtin_noop(VM *vm, size_t arity) {
    (void)arity;                      // unused
    (void)vm;                         // unused
    *list_last(&vm->stack) = TAG_NIL; // overrides the fun, still OK
    return true;
}

bool builtin_call(VM *vm, size_t arity) {
    if (arity != 1) {
        runtime_err(vm, "missing function", 0);
        return false;
    }
    Tag f = *list_last(&vm->stack);
    if (!tag_is_fun(f)) {
        runtime_err_tag(vm, "cannot call non-function: ", f);
        return false;
    }
    if (!call(vm, 0)) {
        return false;
    }
    return true;
}

bool builtin_foreach(VM *vm, size_t arity) {
    if (arity != 2) {
        runtime_err(vm, "foreach takes exactly two arguments", 0);
        return false;
    }
    Tag f = *list_last(&vm->stack);
    if (!tag_is_fun(f)) {
        runtime_err_tag(vm, "foreach expects a function as its second argument; got: ", f);
        return false;
    }
    Tag i = *list_get(&vm->stack, list_len(&vm->stack) - 2);
    switch (tag_type(i)) {
    case TYPE_LIST: {
        List *l = tag_to_list(i);
        list_append(&vm->stack, f);
        for (size_t i = 0; i < list_len(l); i++) {
            list_append(&vm->stack, *list_get(l, i));
            if (!call(vm, 1)) { // replaces the func with the result
                return false;
            }
            *list_last(&vm->stack) = f;
        }
        return true;
    }
    case TYPE_TABLE: {
        runtime_err(vm, "table not implemented yet", 0);
        return false;
    }
    default:
        runtime_err_tag(vm, "foreach expects a list or a table as its first argument; got: ", i);
        return false;
    }
}

bool builtin_stack_trace(VM *vm, size_t arity) {
    size_t skip = 1;
    if (arity == 1) {
        Tag arg = *list_last(&vm->stack);
        int64_t wanted_skip = 0;
        if (!as_int(arg, &wanted_skip)) {
            Tag *err = mem_allocate(sizeof(*err));
            static Slice msg = SLICE("skip must be an integer");
            *err = tag_to_ref(slice_to_tag(&msg));
            error_to_tag(err);
            return false;
        }
        if (wanted_skip < 0) {
            Tag *err = mem_allocate(sizeof(*err));
            static Slice msg = SLICE("skip must be positive");
            *err = tag_to_ref(slice_to_tag(&msg));
            error_to_tag(err);
            return false;
        }
        if ((uint64_t)wanted_skip > SIZE_MAX) {
            Tag *err = mem_allocate(sizeof(*err));
            *err = tag_to_ref(slice_to_tag(&SLICE("skip is too large")));
            error_to_tag(err);
            return false;
        }
        skip = (uint64_t)wanted_skip;
    }
    for (size_t i = 0; i + skip < vm->current_frame; i++) {
        for (size_t j = 0; j < i; j++) {
            putchar(' ');
            putchar(' ');
        }
        const Fun *f = vm->frames[i].f;
        fun_printf(stdout, f);
        if (f->type == FUN_USER) {
            printf(" on line %zu", f->user.line);
        }
        putchar('\n');
    }
    return true;
}

#define BUILTIN(n, f, s)                                                                           \
    (Fun) {                                                                                        \
        .type = FUN_BUILTIN, .builtin = {.name = SLICE(n), .fun = (f), .signature = SLICE(s) }     \
    }

Fun builtins[] = {
    BUILTIN("print", builtin_print, "..."),
    BUILTIN("stack_trace", builtin_stack_trace, "skip=0"),
    BUILTIN("len", builtin_len, "table/list/str"),
    BUILTIN("noop", builtin_noop, "..."),
    BUILTIN("call", builtin_call, "f"),
    BUILTIN("foreach", builtin_foreach, "iterable, fun"),
};

size_t builtins_n = sizeof(builtins) / sizeof(Fun);
