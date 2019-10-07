#ifndef clox_debug_h
#define clox_debug_h

#include "chunk.h"
#include "scanner.h"

void chunkDisassemble(Chunk *chunk, const char *name);
int chunkDisassembleInstr(Chunk *chunk, int offset);
void scannerPrintDebug(Scanner *scanner);

#endif
