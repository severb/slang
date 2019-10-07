#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
  Chunk *chunk;
  uint8_t *ip;

  Value stack[STACK_MAX];
  Value *stackTop;

  Table interned;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vmInit(VM *vm);
void vmFree(VM *vm);

InterpretResult interpret(VM *vm, const char *source);

void vmPush(VM *vm, Value value);
Value vmPop(VM *vm);

#endif
