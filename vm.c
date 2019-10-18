#include <stdarg.h>
#include <stdio.h>

#include "array.h"
#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "intern.h"
#include "str.h"
#include "table.h"
#include "val.h"
#include "vm.h"

VM *vm_init(VM *vm, Table *globals, Intern *intern) {
  if (vm == 0)
    return 0;
  *vm = (VM){
      .chunk = {0},
      .ip = 0,
      .stack = {0},
      .intern = intern,
      .globals = globals,
  };
  array_init(&vm->stack);
  chunk_init(&vm->chunk);
  return vm;
}

void vm_destroy(VM *vm) {
  if (vm == 0)
    return;
  array_destroy(&vm->stack);
  chunk_destroy(&vm->chunk);
  vm_init(vm, vm->globals, vm->intern);
}

static void push(VM *vm, Val *val) { array_append(&vm->stack, val); }

static Val *pop(VM *vm) {
  Val *res = array_pop(&vm->stack);
  assert(res);
  return res;
}

static Val *peek(VM *vm, size_t dist) {
  return &vm->stack.vals[vm->stack.len - dist - 1];
}

static Val *top(VM *vm) { return peek(vm, 0); }

static void runtime_error(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
  size_t instruction = vm->ip - vm->chunk.code;
  int line = vm->chunk.lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);
  array_destroy(&vm->stack);
}

static InterpretResult run(VM *vm) {
// NB: All strings must be interned. Only slices should be stored in chunk's
// constants, globals and on the stack.
#define READ_BYTE() (*vm->ip++)
#define READ_IDX() (READ_BYTE())
#define READ_IDX2() (vm->ip += 2, ((*(vm->ip - 2) << 8) | *(vm->ip - 1)))
#define READ_CONSTANT() (vm->chunk.consts.vals[READ_IDX()])
#define READ_CONSTANT2() (vm->chunk.consts.vals[READ_IDX2()])
#define BINARY_OP(vm, valueType, op, msg)                                      \
  do {                                                                         \
    if (!VAL_IS_NUMBER(*top(vm)) || !VAL_IS_NUMBER(*peek(vm, 1))) {            \
      runtime_error(vm, msg);                                                  \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = pop(vm)->val.as.number;                                         \
    Val *a = top(vm);                                                          \
    a->val.as.number = a->val.as.number op b;                                  \
  } while (false)
#ifdef DEBUG_TRACE_EXECUTION
  printf("== run ==\n");
#endif
  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    if (vm->stack.len == 0)
      printf("EMPTY STACK");
    for (size_t i = 0; i < vm->stack.len; i++) {
      printf("[ ");
      val_print(vm->stack.vals[i]);
      printf(" ]");
    }
    printf("\n");
    chunk_print_dis_instr(&vm->chunk, vm->ip - vm->chunk.code);
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_ADD: {
      Val *right = top(vm);
      Val *left = peek(vm, 1);
      if (VAL_IS_SLICE(*left) && VAL_IS_SLICE(*right)) {
        Val *b = pop(vm);
        Val *a = top(vm);
        *a = intern_str(vm->intern, slice_concat(a->slice, b->slice));
      } else {
        BINARY_OP(vm, VAL_LIT_NUMBER, +,
                  "Operands for '+' must be numbers or strings.");
      }
      break;
    }
    case OP_CONSTANT:
    case OP_CONSTANT2: {
      Val constant;
      if (instruction == OP_CONSTANT)
        constant = READ_CONSTANT();
      else
        constant = READ_CONSTANT2();
      push(vm, &constant);
      break;
    }
    case OP_DEF_GLOBAL:
    case OP_DEF_GLOBAL2: {
      Val constant;
      if (instruction == OP_DEF_GLOBAL)
        constant = READ_CONSTANT();
      else
        constant = READ_CONSTANT2();
      table_set(vm->globals, &constant, pop(vm));
      break;
    }
    case OP_DIVIDE:
      BINARY_OP(vm, VAL_LIT_NUMBER, /, "Operands for '/' must be numbers.");
      break;
    case OP_EQUAL: {
      Val *b = pop(vm);
      Val *a = top(vm);
      *a = VAL_LIT_BOOL(val_equals(*a, *b));
      break;
    }
    case OP_FALSE:
      push(vm, &VAL_LIT_BOOL(false));
      break;
    case OP_GET_GLOBAL:
    case OP_GET_GLOBAL2: {
      Val var;
      if (instruction == OP_GET_GLOBAL)
        var = READ_CONSTANT();
      else
        var = READ_CONSTANT2();
      Val *val = table_get(vm->globals, var);
      if (val == 0) {
        runtime_error(vm, "Undefined variable: %.*s.", var.slice.len,
                      var.slice.c);
        return INTERPRET_RUNTIME_ERROR;
      }
      Val copy = *val;
      push(vm, &copy);
      break;
    }
    case OP_GET_LOCAL:
    case OP_GET_LOCAL2: {
      uint16_t idx;
      if (instruction == OP_GET_LOCAL)
        idx = READ_IDX();
      else
        idx = READ_IDX2();
      Val copy = top(vm)[idx];
      push(vm, &copy);
      break;
    }
    case OP_GREATER:
      BINARY_OP(vm, VAL_LIT_BOOL, >,
                "Operands for comparison must be numbers.");
      break;
    case OP_LESS:
      BINARY_OP(vm, VAL_LIT_BOOL, <,
                "Operands for comparison must be numbers.");
      break;
    case OP_MULTIPLY:
      BINARY_OP(vm, VAL_LIT_NUMBER, *, "Operands for '*' must be numbers.");
      break;
    case OP_NEGATE:
      if (!VAL_IS_NUMBER(*top(vm))) {
        runtime_error(vm, "Operand must be number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      top(vm)->val.as.number *= -1;
      break;
    case OP_NIL:
      push(vm, &VAL_LIT_NIL);
      break;
    case OP_NOT: {
      Val *a = top(vm);
      *a = VAL_LIT_BOOL(!val_truthy(*a));
      break;
    }
    case OP_POP:
      pop(vm);
      break;
    case OP_PRINT:
      val_print(*pop(vm));
      printf("\n");
      break;
    case OP_RETURN: {
      return INTERPRET_OK;
    }
      // case OP_SUBTRACT:
      //      BINARY_OP(vm, LITERAL_NUMBER, -);
      //      break;
    case OP_SET_GLOBAL:
    case OP_SET_GLOBAL2: {
      Val key;
      if (instruction == OP_SET_GLOBAL)
        key = READ_CONSTANT();
      else
        key = READ_CONSTANT2();
      Val key_copy = key;
      Val val = *top(vm);
      if (table_set(vm->globals, &key, &val)) {
        table_del(vm->globals, key_copy);
        runtime_error(vm, "Undefined variable: %.*s.", key_copy.slice.len,
                      key_copy.slice.c);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SET_LOCAL:
    case OP_SET_LOCAL2: {
      uint16_t idx;
      if (instruction == OP_SET_LOCAL)
        idx = READ_IDX();
      else
        idx = READ_IDX2();
      vm->stack.vals[idx] = *top(vm);
      break;
    }
    case OP_TRUE:
      push(vm, &VAL_LIT_BOOL(true));
      break;
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(VM *vm, const char *src) {
  if (!compile(src, &vm->chunk, vm->intern)) {
    return INTERPRET_COMPILE_ERROR;
  }
  chunk_seal(&vm->chunk);
  vm->ip = vm->chunk.code;
#ifdef DEBUG_PRINT_CODE
  printf("== code ==\n");
  chunk_print_dis(&vm->chunk);
#endif
  InterpretResult res = run(vm);
  vm_destroy(vm);
  return res;
}
