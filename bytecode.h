#ifndef clox_bytecode_h
#define clox_bytecode_h

#include "array.h" // arraylist_declare
#include "types.h" // List

#include <assert.h> // assert
#include <stdint.h> // uint8_t

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

void chunk_write(Chunk *, Opcode, Line);
void chunk_write_unary(Chunk *, Opcode, Line, uint64_t);
void chunk_write_binary(Chunk *, Opcode, Line, uint64_t, uint64_t);
Bookmark chunk_reserve(Chunk *, Line l);
Bookmark chunk_reserve_unary(Chunk *, Line l);
Bookmark chunk_reserve_binary(Chunk *, Line l);
void chunk_patch(Chunk *, Bookmark, Opcode);
void chunk_patch_unary(Chunk *, Bookmark, Opcode, uint64_t);
void chunk_patch_binary(Chunk *, Bookmark, Opcode, uint64_t, uint64_t);
size_t chunk_record_const(Chunk *, Val);
void chunk_seal(Chunk *);
void chunk_free(Chunk *);
void chunk_disassamble(const Chunk *);

inline size_t chunk_position(const Chunk *c) {
  return arraylist_len(Opcode)(&c->bytecode);
}

#endif
