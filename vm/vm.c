#include "vm.h"

#include "dynarray.h" // dynarray_get
#include "list.h"     // List
#include "table.h"    // Table

#include <stdint.h> // uint8_t

typedef struct {
  Chunk *chunk;
  uint8_t *ip;
  List stack;
  Table globals;
} VM;

InterpretResult interpret(Chunk *chunk, const char *src) {
  VM vm = (VM) {
    .chunk = chunk,
    .ip = dynarray_get(uint8_t)(&chunk->bytecode, 0)
  };
  (void) vm;
  (void) src;
  return INTERPRET_OK;
}
