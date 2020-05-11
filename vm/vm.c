#include "vm.h"

#include "dynarray.h" // dynarray_get
#include "list.h"     // List
#include "table.h"    // Table

#include <stdint.h> // uint8_t

typedef struct {
  Chunk *chunk;
  size_t ip;
  List stack;
  Table globals;
} VM;

InterpretResult interpret(Chunk *chunk, const char *src) {
  VM vm = {.chunk = chunk, 0};
  return INTERPRET_OK;
}
