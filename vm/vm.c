#include "vm.h"

#include "bytecode.h" // Chunk, chunk_*
#include "list.h"     // List, list_*
#include "mem.h"      // mem_allocate
#include "str.h"      // slice
#include "table.h"    // Table, table_*
#include "tag.h"      // Tag, tag_*, TAG_NIL

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const Chunk *chunk;
    size_t ip;
    List stack;
    List temps;
    Table globals;
} VM;

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
    if (tag_is_i49(key)) {
        i = tag_to_i49(key);
    } else if (tag_is_i64(key)) {
        i = *tag_to_i64(key);
    } else {
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
    // TODO: add break and continue
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
        case OP_SUBTRACT: {
            break; // not used yet
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
        case OP_PRINT: {
            Tag t = pop(vm);
            tag_print(t);
            tag_free(t);
            break;
        }
        case OP_PRINT_NL:
            putchar('\n');
            break;
        case OP_RETURN:
            return true;
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
            // TODO: make globals more like locals and remove late binding
            // the var's name is stored as a string constant, idx is it's index
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
                    runtime_err_tag(vm, "global variable redefinition: ", var);
                } else {
                    // setting a global variable that hasn't yet been defined
                    runtime_err_tag(vm, "undefined global variable: ", var);
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
                runtime_err_tag(vm, "undefined global variable: ", var);
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
            if (pos + 1 == list_len(&vm->stack)) {
                // local declaration
                // when a new variable is declared, it calls OP_SET_LOCAL with the same position as
                // the top of the stack where its initial value is
            } else {
                // local assignment
                *list_get(&vm->stack, pos) = val;
            }
            break;
        }
        case OP_GET_LOCAL: {
            size_t pos = chunk_read_operator(vm->chunk, &vm->ip);
            Tag val = *list_get(&vm->stack, pos);
            assert(!tag_is_own(val)); // only refs are set as locals
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
            if (opcode == OP_SET) {
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
            Tag result = tag_add(read_val, val);
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
        default:
            runtime_err(vm, "bad opcode", 0);
            return false;
        }
    }
}

bool interpret(const Chunk *chunk) {
    VM vm = (VM){.chunk = chunk};
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
