#include "vm.h"

#include "builtins.h" // builtins
#include "bytecode.h" // Chunk, chunk_*
#include "fun.h"      // Fun, fun_*
#include "list.h"     // List, list_*
#include "mem.h"      // mem_allocate
#include "str.h"      // slice
#include "table.h"    // Table, table_*
#include "tag.h"      // Tag, tag_*, TAG_NIL

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void destroy(VM *vm) {
    list_destroy(&vm->stack);
    list_destroy(&vm->temps);
    table_destroy(&vm->globals);
    *vm = (VM){0};
}

static void runtime_err_header(VM *vm) {
    size_t line = chunk_lines_delta(vm->chunk, 0, vm->ip);
    fprintf(stderr, "[line %zu] runtime error: ", line + 1);
}

// TODO: print call stack
static void runtime_err(VM *vm, const char *err, const char *detail) {
    runtime_err_header(vm);
    fputs(err, stderr);
    if (detail) {
        putc('"', stderr);
        fputs(detail, stderr);
        putc('"', stderr);
    }
    putc('\n', stderr);
}

static void runtime_err_tag(VM *vm, const char *err, Tag tag) {
    runtime_err_header(vm);
    fputs(err, stderr);
    tag_reprf(stderr, tag);
    putc('\n', stderr);
}

static void runtime_tag(VM *vm, Tag error) {
    assert(tag_is_error(error));
    runtime_err_header(vm);
    Tag tag = *tag_to_error(error);
    tag_printf(stderr, tag);
    putc('\n', stderr);
}

static inline bool list_key_to_idx(VM *vm, const List *l, Tag key, size_t *idx) {
    int64_t i;
    if (!as_int(key, &i)) {
        runtime_err_tag(vm, "list index is non-integer: ", key);
        return false;
    }
    if (i < 0) {
        runtime_err_tag(vm, "negative index: ", key);
        return false;
    }
    if ((size_t)i >= list_len(l)) {
        runtime_err_tag(vm, "list index out of bounds: ", key);
        return false;
    }
    *idx = (size_t)i;
    return true;
}

static inline void push(VM *vm, Tag t) { list_append(&vm->stack, t); }
static inline Tag pop(VM *vm) { return list_pop(&vm->stack); }
static inline Tag top(VM *vm) { return *list_last(&vm->stack); }
static inline void replace_top(VM *vm, Tag t) { *list_last(&vm->stack) = t; }

#define BINARY_MATH(func)                                                                          \
    do {                                                                                           \
        Tag right = pop(vm);                                                                       \
        Tag left = top(vm);                                                                        \
        Tag result = (func)(left, right);                                                          \
        replace_top(vm, result);                                                                   \
        if (tag_is_error(result)) {                                                                \
            runtime_tag(vm, result);                                                               \
            /* tag_free(result) -- don't free the error, leave it on the  stack */                 \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

// This can be less restrictive if checked during compilation
#ifndef NDEBUG
static_assert(SIZE_MAX >= UINT64_MAX, "cannot cast uint64_t VM operands to size_t");
#endif

// obj and key don't pass ownership
bool item_get(VM *vm, Tag obj, Tag key, Tag *val) {
    if (tag_is_table(obj)) {
        Table *t = tag_to_table(obj);
        if (!table_get(t, key, val)) {
            runtime_err_tag(vm, "key not found: ", key);
            return false;
        };
    } else if (tag_is_list(obj)) {
        List *l = tag_to_list(obj);
        size_t idx;
        if (!list_key_to_idx(vm, l, key, &idx)) {
            return false;
        }
        *val = *list_get(l, idx);
    } else {
        runtime_err(vm, "cannot index type: ", tag_type_str(tag_type(obj)));
        return false;
    }
    return true;
}

// key passes ownership, val is input/output
bool item_set(VM *vm, Tag obj, Tag key, Tag *val) {
    if (tag_is_own(*val)) {
        list_append(&vm->temps, *val);
        *val = tag_to_ref(*val);
    }
    if (tag_is_table(obj)) {
        if (tag_is_own(key)) {
            list_append(&vm->temps, key);
            key = tag_to_ref(key);
        }
        Table *t = tag_to_table(obj);
        table_set(t, key, *val);
    } else if (tag_is_list(obj)) {
        List *l = tag_to_list(obj);
        size_t idx;
        bool idx_success = list_key_to_idx(vm, l, key, &idx);
        tag_free(key);
        if (!idx_success) {
            return false;
        }
        *list_get(l, idx) = *val;
    } else {
        tag_free(key);
        runtime_err(vm, "non indexable type: ", tag_type_str(tag_type(obj)));
        return false;
    }
    return true;
}

static bool run(VM *vm) {
    // TODO: use computed gotos
    for (;;) {
        OpCode opcode = chunk_read_opcode(vm->chunk, vm->ip);
        vm->ip++;
        switch (opcode) {
        case OP_POP:
            tag_free(pop(vm));
            break;
        case OP_POP_N:
            for (size_t i = chunk_read_operator(vm->chunk, &vm->ip); i; i--) {
                tag_free(pop(vm));
            }
            break;
        case OP_ADD: {
            BINARY_MATH(tag_add);
            break;
        }
        case OP_MULTIPLY: {
            BINARY_MATH(tag_mul);
            break;
        }
        case OP_DIVIDE: {
            BINARY_MATH(tag_div);
            break;
        }
        case OP_REMAINDER: {
            BINARY_MATH(tag_mod);
            break;
        }
        case OP_LESS: {
            BINARY_MATH(tag_less);
            break;
        }
        case OP_GREATER: {
            BINARY_MATH(tag_greater);
            break;
        }
        case OP_GET_CONSTANT: {
            size_t idx = chunk_read_operator(vm->chunk, &vm->ip);
            Tag constant = chunk_get_const(vm->chunk, idx);
            if (tag_is_ptr(constant)) {
                constant = tag_to_ref(constant);
            }
            push(vm, constant);
            break;
        }
        case OP_NEGATE:
            replace_top(vm, tag_negate(top(vm)));
            break;
        case OP_TRUE:
            push(vm, TAG_TRUE);
            break;
        case OP_FALSE:
            push(vm, TAG_FALSE);
            break;
        case OP_NIL:
            push(vm, TAG_NIL);
            break;
        case OP_NOT: {
            Tag t = top(vm);
            replace_top(vm, tag_is_true(t) ? TAG_FALSE : TAG_TRUE);
            tag_free(t);
            break;
        }
        case OP_EQUAL: {
            Tag right = pop(vm);
            Tag left = top(vm);
            replace_top(vm, tag_eq(left, right) ? TAG_TRUE : TAG_FALSE);
            tag_free(left);
            tag_free(right);
            break;
        }
        case OP_NOOP:
            break;
        case OP_JUMP_IF_TRUE:
        case OP_JUMP_IF_FALSE: {
            size_t pos = chunk_read_operator(vm->chunk, &vm->ip);
            Tag t = top(vm);
            bool is_true = tag_is_true(t);
            if (is_true == (opcode == OP_JUMP_IF_TRUE)) {
                vm->ip += pos;
            }
            break;
        }
        case OP_JUMP: {
            size_t pos = chunk_read_operator(vm->chunk, &vm->ip);
            vm->ip += pos;
            break;
        }
        case OP_LOOP: {
            // NB: moves back n bytes counting from BEFORE this opcode (this is because the
            // operand's size is variable)
            size_t ip = vm->ip; // don't advance IP
            size_t pos = chunk_read_operator(vm->chunk, &ip);
            assert(vm->ip > pos && "loop before start");
            vm->ip -= pos + 1; // +1 for the opcode itself
            break;
        }
        case OP_DEF_GLOBAL:
        case OP_SET_GLOBAL: {
            // TODO: make globals redefinition a compile-time error
            // var's name is stored as a string constant, idx is it's index
            size_t idx = chunk_read_operator(vm->chunk, &vm->ip);
            Tag var = chunk_get_const(vm->chunk, idx);
            if (tag_is_ptr(var)) {
                var = tag_to_ref(var);
            }
            Tag val = top(vm);
            if (tag_is_own(val)) { // convert to ref before err check to avoid leaks
                list_append(&vm->temps, val);
                val = tag_to_ref(val);
                replace_top(vm, val);
            }
            // table_set returns true on new entries
            if (table_set(&vm->globals, var, val) != (opcode == OP_DEF_GLOBAL)) {
                if (opcode == OP_DEF_GLOBAL) {
                    runtime_err_tag(vm, "global label redefinition: ", var);
                } else {
                    // setting a global variable that hasn't yet been defined
                    runtime_err_tag(vm, "undefined global label: ", var);
                }
                return false;
            }
            if (opcode == OP_DEF_GLOBAL) {
                pop(vm);
            }
            break;
        }
        case OP_GET_GLOBAL: {
            // the var's name is stored as a string constant, idx is it's index
            size_t idx = chunk_read_operator(vm->chunk, &vm->ip);
            Tag var = chunk_get_const(vm->chunk, idx);
            // is OK for var to be owned, we're using it just for table lookup
            Tag val;
            if (table_get(&vm->globals, var, &val)) {
                assert(!tag_is_own(val)); // only refs are set as globals
                push(vm, val);
            } else {
                runtime_err_tag(vm, "undefined global label: ", var);
                return false;
            }
            break;
        }
        case OP_SET_LOCAL: {
            Tag val = top(vm);
            if (tag_is_own(val)) {
                list_append(&vm->temps, val);
                val = tag_to_ref(val);
                replace_top(vm, val);
            }
            size_t pos = chunk_read_operator(vm->chunk, &vm->ip);
            if (pos + vm->frame_base + 1 == list_len(&vm->stack)) {
                // local declaration
                // when a new variable is declared, it calls OP_SET_LOCAL with the same position as
                // the top of the stack where its initial value is
            } else {
                // local assignment
                *list_get(&vm->stack, pos + vm->frame_base) = val;
            }
            break;
        }
        case OP_GET_LOCAL: {
            size_t pos = chunk_read_operator(vm->chunk, &vm->ip);
            Tag val = *list_get(&vm->stack, pos + vm->frame_base);
            // only refs are local because we POP_N them (or return early) without cleanup
            assert(!tag_is_own(val) && "local is not ref");
            push(vm, val);
            break;
        }
        case OP_DICT: {
            Table *tab = mem_allocate(sizeof(*tab));
            *tab = (Table){0};
            Tag tag = table_to_tag(tab);
            push(vm, tag);
            break;
        }
        case OP_LIST: {
            List *list = mem_allocate(sizeof(*list));
            *list = (List){0};
            Tag tag = list_to_tag(list);
            push(vm, tag);
            break;
        }
        case OP_LIST_INIT: {
            // leave the list on top of the stack
            Tag val = pop(vm);
            if (tag_is_own(val)) {
                list_append(&vm->temps, val);
                val = tag_to_ref(val);
            }
            // conversion should always succeed unless this is bad bytecode
            List *l = tag_to_list(top(vm));
            list_append(l, val);
            break;
        }
        case OP_APPEND: {
            Tag val = pop(vm);
            if (tag_is_own(val)) { // convert to ref before error check to avoid ref leak
                list_append(&vm->temps, val);
                val = tag_to_ref(val);
            }
            Tag list = top(vm);
            if (!tag_is_list(list)) {
                runtime_err(vm, "non-appendable type: ", tag_type_str(tag_type(list)));
                return false;
            }
            List *l = tag_to_list(list);
            list_append(l, val);
            tag_free(list); // [] []= 1;
            replace_top(vm, val);
            break;
        }
        case OP_DICT_INIT:
        case OP_ITEM_SET: {
            Tag val = pop(vm);
            Tag key = pop(vm);
            Tag obj = top(vm);
            if (!item_set(vm, obj, key, &val)) {
                tag_free(val);
                return false;
            };
            if (opcode == OP_ITEM_SET) {
                tag_free(obj); // ({})[0] = 0;
                replace_top(vm, val);
            }
            break;
        }
        case OP_ITEM_GET: {
            Tag key = pop(vm);
            Tag obj = top(vm);
            Tag val;
            bool item_get_success = item_get(vm, obj, key, &val);
            tag_free(key);
            if (!item_get_success) {
                return false;
            }
            tag_free(obj); // ([1,2,3])[0];
            replace_top(vm, val);
            break;
        }
        case OP_ITEM_SHORT_REMAINDER:
        case OP_ITEM_SHORT_MULTIPLY:
        case OP_ITEM_SHORT_DIVIDE:
        case OP_ITEM_SHORT_ADD: {
            Tag val = pop(vm);
            Tag key = pop(vm);
            Tag obj = top(vm);
            Tag read_val;
            if (!item_get(vm, obj, key, &read_val)) {
                tag_free(key);
                tag_free(val);
                return false;
            }
            Tag result = TAG_NIL;
            switch (opcode) {
            case OP_ITEM_SHORT_REMAINDER:
                result = tag_mod(read_val, val);
                break;
            case OP_ITEM_SHORT_MULTIPLY:
                result = tag_mul(read_val, val);
                break;
            case OP_ITEM_SHORT_DIVIDE:
                result = tag_div(read_val, val);
                break;
            case OP_ITEM_SHORT_ADD:;
                result = tag_add(read_val, val);
                break;
            default:
                assert(0 && "unhandled short-hand assignment operator");
            }
            if (tag_is_error(result)) {
                runtime_tag(vm, result);
                tag_free(key);
                tag_free(result);
                return false;
            }
            if (!item_set(vm, obj, key, &result)) {
                tag_free(result);
                return false;
            }
            tag_free(obj);
            replace_top(vm, result);
            break;
        }
        case OP_CALL: {
            // TODO: make builtins play well with callbacks and stack traces
            // TODO: clean this up a bit
            size_t arity = chunk_read_operator(vm->chunk, &vm->ip);
            size_t len = list_len(&vm->stack);
            assert(len > arity && "bad arity call");
            Tag t = *list_get(&vm->stack, len - arity - 1);
            if (!tag_is_fun(t)) {
                runtime_err(vm, "cannot call type", tag_type_str(tag_type(t)));
                return false;
            }
            if (vm->current_frame >= MAX_FRAMES) {
                runtime_err(vm, "call stack depth exceeded", 0);
                return false;
            }
            Fun *f = tag_to_fun(t);
            if (f->type == FUN_USER) {
                if (f->user.arity != arity) {
                    // TODO: improve bad function arity error message
                    // include the signature and line where it's defined
                    runtime_err(vm, "function called with bad arity", 0);
                    return false;
                }
            }
            // change all arguments to refs because locals don't expect owned pointers
            // (see note on OP_GET_LOCAL)
            for (size_t i = len - arity; i < len; i++) {
                Tag *t = list_get(&vm->stack, i);
                if (tag_is_own(*t)) {
                    list_append(&vm->temps, *t);
                    *t = tag_to_ref(*t);
                }
            }
            if (f->type == FUN_USER) {
                vm->frames[vm->current_frame++] = (CallFrame){
                    .f = f,
                    .prev_ip = vm->ip,
                    .prev_frame_base = vm->frame_base,
                };
                vm->ip = f->user.entry;
                vm->frame_base = len - arity;
            } else {
                // TODO: record builtins in frames for tracebacks
                Tag result = f->builtin.fun(vm, arity);
                list_trunc(&vm->stack, len - arity);
                replace_top(vm, result);
                if (tag_is_error(result)) {
                    runtime_tag(vm, result);
                    return false;
                }
            }
            break;
        }
        case OP_RETURN: {
            if (vm->current_frame == 0) {
                return true; // halt
            }
            Tag result = top(vm);
            CallFrame *frame = &vm->frames[--vm->current_frame];
            // NB: because we trunc the stack, we don't pop and free the values
            // this seems fine because locals don't own values, but maybe it
            // isn't
            list_trunc(&vm->stack, vm->frame_base);
            replace_top(vm, result); // replace the function with the result
            vm->frame_base = frame->prev_frame_base;
            vm->ip = frame->prev_ip;
            break;
        }
        default:
            runtime_err(vm, "bad opcode", 0);
            return false;
        }
    }
}

void register_globals(Table *globals) {
    for (size_t i = 0; i < builtins_n; i++) {
        Fun *fun = &builtins[i];
        Tag name = slice_to_tag(&fun->builtin.name);
        table_set(globals, tag_to_ref(name), tag_to_ref(fun_to_tag(fun)));
    }
}

bool interpret(const Chunk *chunk) {
    VM vm = (VM){.chunk = chunk};
    register_globals(&vm.globals);
    bool result = run(&vm);
#ifdef SLANG_DEBUG
    fputs("temps: ", stdout);
    list_print(&vm.temps);
    putchar('\n');
    fputs("stack: ", stdout);
    list_print(&vm.stack);
    putchar('\n');
    fputs("globals: ", stdout);
    table_print(&vm.globals);
    putchar('\n');
#endif
    destroy(&vm);
    return result;
}
