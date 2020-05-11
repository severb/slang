#ifndef slang_vm_h
#define slang_vm_h

#include "bytecode.h" // Chunk

typedef enum {
  INTERPRET_OK,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

InterpretResult interpret(Chunk *, const char *src);

#endif
