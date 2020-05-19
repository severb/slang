#include "vm.h"

#include "bytecode.h" // Chunk, chunk_*
#include "list.h"     // List, list_*
#include "table.h"    // Table
#include "tag.h"      // Tag, tag_*

#include <stdint.h> // uint8_t
#include <stdio.h>  // stderr, fputc, putchar

typedef struct {
  const Chunk *chunk;
  size_t ip;
  List stack;
  Table globals;
} VM;

static void destroy(VM *vm) {
  list_destroy(&vm->stack);
  table_destroy(&vm->globals);
  *vm = (VM){0};
}

static void print_runtime_error(VM *vm, Tag error) {
  size_t line = chunk_lines_delta(vm->chunk, 0, vm->ip);
  fprintf(stderr, "[line %zu] runtime ", line + 1);
  tag_printf(stderr, error);
  fputc('\n', stderr);
}

static inline void push(VM *vm, Tag t) { list_append(&vm->stack, t); }
static inline Tag pop(VM *vm) { return list_pop(&vm->stack); }
static inline Tag top(VM *vm) { return *list_last(&vm->stack); }
static inline void replace_top(VM *vm, Tag t) { *list_last(&vm->stack) = t; }

static bool run(VM *vm) {
  for (;;) {
    uint8_t opcode = chunk_read_opcode(vm->chunk, vm->ip);
    vm->ip++;
    switch (opcode) {
    case OP_ADD: {
      Tag right = pop(vm);
      Tag left = top(vm);
      Tag result = tag_add(left, right);
      if (tag_is_error(result)) {
        print_runtime_error(vm, result);
        tag_free(result);
        return false;
      }
      replace_top(vm, result);
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
    }
  }
}

bool interpret(const Chunk *chunk) {
  VM vm = (VM){.chunk = chunk};
  bool result = run(&vm);
  destroy(&vm);
  return result;
}
