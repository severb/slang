#ifndef clox_bytecode_h
#define clox_bytecode_h

#include "array.h" // arraylist_declare
#include "types.h" // List

#include <assert.h> // assert
#include <stdint.h> // uint8_t, SIZE_MAX

#define OPCODE(ENUM) ENUM,
typedef enum {
#include "bytecode_ops.incl"
} OpCode;
#undef OPCODE

typedef uint8_t Opcode;
arraylist_declare(Opcode);
typedef ArrayList(Opcode) Bytecode;

typedef size_t Line;
arraylist_declare(Line);
typedef ArrayList(Line) LineIdx;

typedef struct {
  Bytecode bytecode;
  LineIdx lines;
  List consts;
} Chunk;

typedef size_t Bookmark;

void chunk_write_operation(Chunk *, Line, Opcode);
void chunk_write_operand(Chunk *, Line, uint64_t);
void chunk_write_unary(Chunk *, Line, Opcode, uint64_t);
Bookmark chunk_reserve_operation(Chunk *, Line l);
Bookmark chunk_reserve_operand(Chunk *, Line l);
Bookmark chunk_reserve_unary(Chunk *, Line l);
void chunk_patch_operation(Chunk *, Bookmark, Opcode);
void chunk_patch_operand(Chunk *, Bookmark, uint64_t);
void chunk_patch_unary(Chunk *, Bookmark, Opcode, uint64_t);
size_t chunk_record_const(Chunk *, Val);
void chunk_seal(Chunk *);
void chunk_free(Chunk *);
void chunk_disassamble(const Chunk *);

inline size_t chunk_len(const Chunk *c) {
  return arraylist_len(Opcode)(&c->bytecode);
}

inline Opcode chunk_read_opcode(const Chunk *c, size_t offset) {
  return *arraylist_get(Opcode)(&c->bytecode, offset);
}

inline uint64_t chunk_read_operator(const Chunk *c, size_t *offset) {
  assert(*offset < chunk_len(c));
  uint64_t result = *arraylist_get(Opcode)(&c->bytecode, (*offset)++);
  if (!(result & 0x80)) {
    return result;
  }
  result &= 0x7f;
  for (int i = 1; i < 8; i++) {
    uint64_t byte = *arraylist_get(Opcode)(&c->bytecode, (*offset)++);
    if (!(byte & 0x80)) {
      return result | (byte << (7 * i));
    }
    result |= (byte & 0x7f) << (7 * i);
  }
  uint64_t byte = *arraylist_get(Opcode)(&c->bytecode, (*offset)++);
  return result | (byte << 56);
}

#endif
