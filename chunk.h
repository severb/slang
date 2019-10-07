#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

typedef enum {
  OP_ADD,
  OP_CONSTANT,
  OP_DIVIDE,
  OP_EQUAL,
  OP_FALSE,
  OP_GREATER,
  OP_LESS,
  OP_MULTIPLY,
  OP_NEGATE,
  OP_NIL,
  OP_NOT,
  OP_RETURN,
  // OP_SUBTRACT,
  OP_TRUE,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t *code;
  int *lines; // each entry correspons to an entry in code
  ValueArray constants;
} Chunk;

void chunkInit(Chunk *chunk);
void chunkFree(Chunk *chunk);
void chunkWrite(Chunk *chunk, uint8_t byte, int line);
int chunkAddConst(Chunk *chunk, Value value);

#endif
