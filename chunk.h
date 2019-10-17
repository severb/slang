#ifndef clox_chunk_h
#define clox_chunk_h

#include "array.h"
#include "common.h"
#include "table.h"
#include "val.h"

#define FOREACH_OPCODE(OPCODE)                                                 \
  OPCODE(OP_ADD)                                                               \
  OPCODE(OP_CONSTANT)                                                          \
  OPCODE(OP_CONSTANT2)                                                         \
  OPCODE(OP_DEF_GLOBAL)                                                        \
  OPCODE(OP_DEF_GLOBAL2)                                                       \
  OPCODE(OP_DIVIDE)                                                            \
  OPCODE(OP_EQUAL)                                                             \
  OPCODE(OP_FALSE)                                                             \
  OPCODE(OP_GET_GLOBAL)                                                        \
  OPCODE(OP_GET_GLOBAL2)                                                       \
  OPCODE(OP_GREATER)                                                           \
  OPCODE(OP_LESS)                                                              \
  OPCODE(OP_MULTIPLY)                                                          \
  OPCODE(OP_NEGATE)                                                            \
  OPCODE(OP_NIL)                                                               \
  OPCODE(OP_NOT)                                                               \
  OPCODE(OP_POP)                                                               \
  OPCODE(OP_PRINT)                                                             \
  OPCODE(OP_RETURN)                                                            \
  OPCODE(OP_SET_GLOBAL)                                                        \
  OPCODE(OP_SET_GLOBAL2)                                                       \
  OPCODE(OP_SUBTRACT)                                                          \
  OPCODE(OP_TRUE)

#define GENERATE_ENUM(ENUM) ENUM,
typedef enum { FOREACH_OPCODE(GENERATE_ENUM) } OpCode;
#undef GENERATE_ENUM

typedef struct {
  size_t cap;
  size_t len;
  uint8_t *code;
  // TODO: use a run-length encoding for lines
  // until then, line[i] -> code[i]
  size_t *lines;
  Array consts;
  Table const_pos;
} Chunk;

Chunk *chunk_init(Chunk *);
void chunk_destroy(Chunk *);

void chunk_write(Chunk *, uint8_t, size_t line);
void chunk_write2(Chunk *, uint8_t[2], size_t line);
void chunk_write3(Chunk *, uint8_t[3], size_t line);
void chunk_emit_const(Chunk *, OpCode, Val *, size_t line);

// chunk_seal() releases the memory held by const_pos table, and the consts,
// code and lines arrays. After the chunk is sealed, newly added constants can
// create duplicates.
void chunk_seal(Chunk *);

size_t chunk_print_dis_instr(const Chunk *, size_t offset);
void chunk_print_dis(const Chunk *);

#endif
