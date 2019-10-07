#include <stdlib.h>

#include "chunk.h"
#include "common.h"
#include "memory.h"

void chunkInit(Chunk *chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  valueArrayInit(&chunk->constants);
}

void chunkFree(Chunk *chunk) {
  FREE_ARRAY(chunk->code, uint8_t, chunk->capacity);
  FREE_ARRAY(chunk->lines, int, chunk->capacity);
  valueArrayFree(&chunk->constants);
  chunkInit(chunk);
}

void chunkWrite(Chunk *chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code =
        GROW_ARRAY(chunk->code, uint8_t, oldCapacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(chunk->lines, int, oldCapacity, chunk->capacity);
  }
  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

int chunkAddConst(Chunk *chunk, Value value) {
  valueArrayWrite(&chunk->constants, value);
  return chunk->constants.count - 1;
}
