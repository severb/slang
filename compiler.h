#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "memory.h"
#include "table.h"

bool compile(const char *source, Chunk *chunk, Table *table);

#endif
