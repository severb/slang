#ifndef clox_compiler_h
#define clox_compiler_h

#include "chunk.h"
#include "intern.h"

bool compile(const char *, Chunk *, Intern *);

#endif
