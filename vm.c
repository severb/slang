#include <stdarg.h>
#include <stdio.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

// TODO: Minimize pop-ing the stack.
// Unary operations should mutate the top of the stack while binary operations
// should only pop once.

static void resetStack(VM *vm) { vm->stackTop = &vm->stack[0]; }

void vmInit(VM *vm) {
  resetStack(vm);
  tableInit(&vm->interned);
}

void vmFree(VM *vm) {
  resetStack(vm);
  tableFreeKeys(&vm->interned);
  tableFree(&vm->interned);
}

void vmPush(VM *vm, Value value) {
  *vm->stackTop = value;
  vm->stackTop++;
}

Value vmPop(VM *vm) {
  vm->stackTop--;
  return *vm->stackTop;
}

static Value peek(VM *vm, int distance) { return vm->stackTop[-1 - distance]; }

static void runtimeError(VM *vm, const char *format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = vm->ip - vm->chunk->code;
  int line = vm->chunk->lines[instruction];
  fprintf(stderr, "[line %d] in script\n", line);

  resetStack(vm);
}

static bool isFalsy(Value value) {
  if (VAL_IS_NIL(value))
    return true;
  if (VAL_IS_BOOL(value))
    return !VAL_AS_BOOL(value);
  return false;
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->ip++)
#define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])
#define BINARY_OP(vm, valueType, op, msg)                                      \
  do {                                                                         \
    if (!VAL_IS_NUMBER(peek(vm, 0)) || !VAL_IS_NUMBER(peek(vm, 1))) {          \
      runtimeError(vm, msg);                                                   \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = VAL_AS_NUMBER(vmPop(vm));                                       \
    double a = VAL_AS_NUMBER(vmPop(vm));                                       \
    vmPush(vm, valueType(a op b));                                             \
  } while (false)
  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    if (vm->stack == vm->stackTop) {
      printf("EMPTY STACK");
    }
    for (Value *slot = vm->stack; slot < vm->stackTop; slot++) {
      printf("[ ");
      valuePrint(*slot);
      printf(" ]");
    }
    printf("\n");
    chunkDisassembleInstr(vm->chunk, (int)(vm->ip - vm->chunk->code));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_ADD:
      if (VAL_IS_OBJANYSTRING(peek(vm, 0)) &&
          VAL_IS_OBJANYSTRING(peek(vm, 1))) {
        Obj *b = VAL_AS_OBJ(vmPop(vm));
        Obj *a = VAL_AS_OBJ(vmPop(vm));
        Obj *result = internStringsConcat(&vm->interned, a, b);
        vmPush(vm, VAL_LIT_OBJ(result));
      } else {
        BINARY_OP(vm, VAL_LIT_NUMBER, +,
                  "Operands for '+' must be numbers or strings.");
      }
      break;
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      vmPush(vm, constant);
      break;
    }
    case OP_DIVIDE:
      BINARY_OP(vm, VAL_LIT_NUMBER, /, "Operands for '/' must be numbers.");
      break;
    case OP_EQUAL: {
      Value b = vmPop(vm), a = vmPop(vm);
      vmPush(vm, VAL_LIT_BOOL(valuesEqual(a, b)));
      break;
    }
    case OP_FALSE:
      vmPush(vm, VAL_LIT_BOOL(false));
      break;
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
      if (!VAL_IS_NUMBER(peek(vm, 0))) {
        runtimeError(vm, "Operand must be number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      vmPush(vm, VAL_LIT_NUMBER(-VAL_AS_NUMBER(vmPop(vm))));
      break;
    case OP_NIL:
      vmPush(vm, VAL_LIT_NIL());
      break;
    case OP_NOT:
      vmPush(vm, VAL_LIT_BOOL(isFalsy(vmPop(vm))));
      break;
    case OP_RETURN: {
      valuePrint(vmPop(vm));
      printf("\n");
      return INTERPRET_OK;
    }
      // case OP_SUBTRACT:
      //      BINARY_OP(vm, LITERAL_NUMBER, -);
      //      break;
    case OP_TRUE:
      vmPush(vm, VAL_LIT_BOOL(true));
      break;
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(VM *vm, const char *source) {
  Chunk chunk;
  chunkInit(&chunk);

  if (!compile(source, &chunk, &vm->interned)) {
    chunkFree(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = chunk.code;

#ifdef DEBUG_TRACE_EXECUTION
  printf("== run ==\n");
#endif
  InterpretResult result = run(vm);

  chunkFree(&chunk);
  return result;
}
