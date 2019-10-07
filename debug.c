#include <stdio.h>

#include "chunk.h"
#include "debug.h"
#include "scanner.h"
#include "value.h"

static int simpleInstruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constantInstruction(const char *name, Chunk *chunk, int offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  valuePrint(chunk->constants.values[constant]);
  printf("'\n");
  return offset + 2;
}

int chunkDisassembleInstr(Chunk *chunk, int offset) {
  printf("%04d ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", chunk->lines[offset]);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_ADD:
    return simpleInstruction("OP_ADD", offset);
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_DIVIDE:
    return simpleInstruction("OP_DIVIDE", offset);
  case OP_EQUAL:
    return simpleInstruction("OP_EQUAL", offset);
  case OP_FALSE:
    return simpleInstruction("OP_FALSE", offset);
  case OP_GREATER:
    return simpleInstruction("OP_GREATER", offset);
  case OP_LESS:
    return simpleInstruction("OP_LESS", offset);
  case OP_MULTIPLY:
    return simpleInstruction("OP_MULTIPLY", offset);
  case OP_NEGATE:
    return simpleInstruction("OP_NEGATE", offset);
  case OP_NIL:
    return simpleInstruction("OP_NIL", offset);
  case OP_NOT:
    return simpleInstruction("OP_NOT", offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
    //  case OP_SUBTRACT:
    //    return simpleInstruction("OP_SUBTRACT", offset);
  case OP_TRUE:
    return simpleInstruction("OP_TRUE", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

void chunkDisassemble(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = chunkDisassembleInstr(chunk, offset);
  }
}

void scannerPrintDebug(Scanner *scanner) {
  int line = -1;
  for (;;) {
    Token token = scannerConsumeToken(scanner);
    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    printf("%17s %2d '%.*s'\n", TOKEN_TO_STRING[token.type], token.type,
           token.length, token.start);

    if (token.type == TOKEN_EOF)
      break;
  }
}
