#ifndef clox_compiler_h
#define clox_compiler_h

#include "frontend/bytecode.h"

#include <stdbool.h>

bool compile(const char *, Chunk *);

#endif
