#include "vm.h"

#include "bytecode.h" // Chunk, chunk_*
#include "dynarray.h" // dynarray_get
#include "list.h"     // List, list_*
#include "table.h"    // Table
#include "tag.h"

#include <stdarg.h> // va_list, va_start, va_end
#include <stddef.h> // ptrdiff_t
#include <stdint.h> // uint8_t
#include <stdio.h>  // vfprintf, stderr, fputs

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
  fputs("\n", stderr);
}

static inline void push(VM *vm, Tag t) { list_append(&vm->stack, t); }
static inline Tag pop(VM *vm) { return list_pop(&vm->stack); }
static inline Tag peek(VM *vm, size_t dist) {
  assert(list_len(&vm->stack) > dist);
  return *list_get(&vm->stack, list_len(&vm->stack) - dist - 1);
}
static inline Tag top(VM *vm) { return peek(vm, 0); }
static inline void pop_push(VM *vm, Tag t) { *list_last(&vm->stack) = t; }
static inline void pop_pop_push(VM *vm, Tag t) {
  list_pop(&vm->stack);
  pop_push(vm, t);
}

static bool run(VM *vm) {
  for (;;) {
    uint8_t opcode = chunk_read_opcode(vm->chunk, vm->ip);
    vm->ip++;
    switch (opcode) {
    case OP_ADD: {
      Tag right = top(vm);
      Tag left = peek(vm, 1);
      Tag result = tag_add(left, right);
      if (tag_is_error(result)) {
        print_runtime_error(vm, result);
        tag_free(result);
        return false;
      }
      pop_pop_push(vm, result);
      break;
    }
    case OP_GET_CONSTANT: {
      size_t idx = chunk_read_operator(vm->chunk, &vm->ip);
      push(vm, chunk_get_const(vm->chunk, idx));
      break;
    }
    case OP_PRINT:
      tag_print(pop(vm));
      break;
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
