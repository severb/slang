/*
#ifndef clox_vm_h
#define clox_vm_h

#include "array.h"
#include "chunk.h"
#include "common.h"
#include "intern.h"
#include "table.h"

typedef struct {
  Chunk chunk;
  uint8_t *ip;
  Array stack;
  Intern *intern;
  Table *globals;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

VM *vm_init(VM *, Table *globals, Intern *);
void vm_destroy(VM *);

InterpretResult interpret(VM *, const char *src);

#endif
*/