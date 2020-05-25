#include "vm.h"

#include "bytecode.h" // Chunk, chunk_*
#include "list.h"     // List, list_*
#include "str.h"      // slice
#include "table.h"    // Table, table_*
#include "tag.h"      // Tag, tag_*, TAG_NIL

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

static void print_runtime_error(VM *vm, Tag error, Tag var) {
  size_t line = chunk_lines_delta(vm->chunk, 0, vm->ip);
  fprintf(stderr, "[line %zu] runtime ", line + 1);
  tag_printf(stderr, error);
  if (!tag_biteq(var, TAG_NIL)) {
    putc('"', stderr);
    tag_printf(stderr, var);
    putc('"', stderr);
  }
  putc('\n', stderr);
}

static const char undef_var_error[] = "undefined variable ";
static const char glob_redef_error[] = "global variable redefinition ";

static void print_var_error(VM *vm, const char *msg, size_t len, Tag var) {
  Slice s = slice(msg, msg + len);
  Tag s_tag = slice_to_tag(&s);
  Tag err = error_to_tag(&s_tag);
  print_runtime_error(vm, err, var);
}

static void print_bad_opcode(VM *vm) {
  const char msg[] = "bad opcode";
  Slice s = slice(msg, msg + sizeof(msg) - 1);
  Tag s_tag = slice_to_tag(&s);
  Tag err = error_to_tag(&s_tag);
  print_runtime_error(vm, err, TAG_NIL);
}

static inline void push(VM *vm, Tag t) { list_append(&vm->stack, t); }
static inline Tag pop(VM *vm) { return list_pop(&vm->stack); }
static inline Tag top(VM *vm) { return *list_last(&vm->stack); }
static inline void replace_top(VM *vm, Tag t) { *list_last(&vm->stack) = t; }

#define BINARY_MATH(func)                                                      \
  do {                                                                         \
    Tag right = pop(vm);                                                       \
    Tag left = top(vm);                                                        \
    Tag result = (func)(left, right);                                          \
    replace_top(vm, result);                                                   \
    if (tag_is_error(result)) {                                                \
      print_runtime_error(vm, result, TAG_NIL);                                \
      /* tag_free(result) -- don't free the error, leave it on the  stack */   \
      return false;                                                            \
    }                                                                          \
  } while (0)

static bool run(VM *vm) {
  for (;;) {
    OpCode opcode = chunk_read_opcode(vm->chunk, vm->ip);
    vm->ip++;
    switch (opcode) {
    case OP_POP:
      pop(vm);
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
      replace_top(vm, tag_equals(left, right));
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
      // NB: moves back n bytes from before this opcode (not counting the
      // operand's size which can be variable)
      size_t ip = vm->ip; // don't advance IP
      size_t pos = chunk_read_operator(vm->chunk, &ip);
      assert(vm->ip > pos && "loop before start");
      vm->ip -= pos + 1; // +1 for the opcode itself
      break;
    }
    case OP_DEF_GLOBAL:
    case OP_SET_GLOBAL: {
      size_t idx = chunk_read_operator(vm->chunk, &vm->ip);
      Tag var = chunk_get_const(vm->chunk, idx);
      if (tag_is_ptr(var)) {
        var = tag_to_ref(var);
      }
      Tag val = top(vm);
      if (tag_is_own(val)) {
        list_append(&vm->temps, val);
        val = tag_to_ref(val);
        replace_top(vm, val);
      }
      // table_set returns true on new entries
      if (table_set(&vm->globals, var, val) != (opcode == OP_DEF_GLOBAL)) {
        if (opcode == OP_DEF_GLOBAL) {
          // TODO: make global redefinition a compile-time error
          print_var_error(vm, glob_redef_error, sizeof(glob_redef_error) - 1,
                          var);
        } else {
          print_var_error(vm, undef_var_error, sizeof(undef_var_error) - 1,
                          var);
        }
        return false;
      }
      if (opcode == OP_DEF_GLOBAL) {
        pop(vm);
      }
      break;
    }
    case OP_GET_GLOBAL: {
      size_t idx = chunk_read_operator(vm->chunk, &vm->ip);
      Tag var = chunk_get_const(vm->chunk, idx);
      Tag val;
      if (table_get(&vm->globals, var, &val)) {
        if (tag_is_own(val)) {
          val = tag_to_ref(val);
        }
        push(vm, val);
      } else {
        print_var_error(vm, undef_var_error, sizeof(undef_var_error) - 1, var);
        return false;
      }
      break;
    }
    case OP_SET_LOCAL: {
      Tag val = top(vm);
      if (tag_is_own(val)) {
        list_append(&vm->temps, val);
        replace_top(vm, tag_to_ref(val));
      }
      size_t pos = chunk_read_operator(vm->chunk, &vm->ip);
      if (pos + 1 != list_len(&vm->stack)) {
        // assignment
        *list_get(&vm->stack, pos) = pop(vm);
      } else {
        // declaration
        // when a new variable is declared, it calls OP_SET_LOCAL with the same
        // position as the top of the stack where its initial value is
      }
      break;
    }
    case OP_GET_LOCAL: {
      size_t pos = chunk_read_operator(vm->chunk, &vm->ip);
      push(vm, *list_get(&vm->stack, pos));
      break;
    }
    default:
      print_bad_opcode(vm);
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
