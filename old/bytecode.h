#ifndef clox_chunk_h
#define clox_chunk_h

#include "table.h" // Table

#include <stdbool.h> // bool
#include <stdint.h>  // uint32_t, uint8_t

#define OPCODE(ENUM) ENUM,
typedef enum {
#include "opcodes.enum"
} OpCode;
#undef OPCODE

typedef struct {
  size_t cap;
  size_t len;
  uint8_t *code;
} Bytecode;

typedef struct {
  size_t cap;
  size_t len;
  uint32_t *lines;
} LineIdx;

typedef struct {
  Bytecode bytecode;
  LineIdx lines;
  Val *consts;
} Chunk;

typedef struct {
  Chunk *chunk;
  size_t cursor;
} Bookmark;

Chunk *chunk_init(Chunk *);
void chunk_destroy(Chunk *);

bool chunk_write(Chunk *, uint8_t, uint32_t line);
bool chunk_writen(Chunk *, uint8_t const *, uint8_t n, uint32_t line);

// chunk_write_op() writes op, op+1, op+2, op+3 based on the operand's size.
bool chunk_write_op(Chunk *, uint8_t op, uint32_t operand, uint32_t line);

Bookmark chunk_bookmark(Chunk *);
void bookmark_patch_left(Bookmark);
void bookmark_patch_right(Bookmark);

// chunk_finalize() creates the consts array and frees the const_idx table.
void chunk_finalize(Chunk *);

uint32_t chunk_print_dis_instr(Chunk const *, uint32_t offset);
void chunk_print_dis(Chunk const *);

#endif
