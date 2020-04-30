#ifndef clox_compiler_h
#define clox_compiler_h

#include "bytecode.h"

#include <stdbool.h>

bool compile(const char *, Chunk *);

#endif
