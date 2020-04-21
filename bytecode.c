#include "bytecode.h"

#include "array.h" // arraylist_*
#include "types.h" // list_*
#include "val.h"   // Val, val_*

#include <assert.h>   // assert
#include <inttypes.h> // PRI*
#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t, SIZE_MAX

arraylist_define(Opcode);
arraylist_define(Line);

static void write_byte(Chunk *c, Line l, uint8_t b) {
  arraylist_append(Opcode)(&c->bytecode, &b);
  while (l >= arraylist_len(Line)(&c->lines)) {
    size_t len = arraylist_len(Line)(&c->lines);
    Line prev = len > 0 ? *arraylist_get(Line)(&c->lines, len - 1) : 0;
    arraylist_append(Line)(&c->lines, &prev);
  }
  *arraylist_get(Line)(&c->lines, arraylist_len(Line)(&c->lines) - 1) += 1;
}

void chunk_write_operation(Chunk *c, Line l, Opcode op) {
  write_byte(c, l, op);
}

// least significant byte 1st, max 9 bytes, 9th byte not tagged
void chunk_write_operand(Chunk *c, Line l, uint64_t operand) {
  for (int i = 0; i < 8; i++) {
    if (operand < 0x80) {
      write_byte(c, l, operand);
      return;
    }
    write_byte(c, l, 0x80 | (operand & 0x7f));
    operand >>= 7;
  }
  write_byte(c, l, operand); // last full byte
}

void chunk_write_unary(Chunk *c, Line l, Opcode op, uint64_t operand) {
  chunk_write_operation(c, l, op);
  chunk_write_operand(c, l, operand);
}

size_t chunk_reserve_operation(Chunk *c, Line l) {
  size_t idx = arraylist_len(Opcode)(&c->bytecode);
  chunk_write_operation(c, l, OP_NOOP);
  return idx;
}

size_t chunk_reserve_operand(Chunk *c, Line l) {
  size_t idx = arraylist_len(Opcode)(&c->bytecode);
  for (int i = 0; i < 9; i++) { // 8 + 1 bytes max operand size
    chunk_write_operation(c, l, OP_NOOP);
  }
  return idx;
}

size_t chunk_reserve_unary(Chunk *c, Line l) {
  size_t idx = chunk_reserve_operation(c, l);
  chunk_reserve_operand(c, l);
  return idx;
}

static void patch_byte(Chunk *c, size_t idx, uint8_t b) {
  assert(idx < arraylist_len(Opcode)(&c->bytecode));
  *arraylist_get(Opcode)(&c->bytecode, idx) = b;
}

void chunk_patch_operation(Chunk *c, Bookmark idx, Opcode op) {
  patch_byte(c, idx, op);
}

void chunk_patch_operand(Chunk *c, Bookmark idx, uint64_t operand) {
  assert(idx < SIZE_MAX - 9);
  for (int i = 0; i < 8; i++) {
    patch_byte(c, idx + i, 0x80 | (0x7F & operand));
    operand >>= 7;
  }
  patch_byte(c, idx + 8, operand);
}

void chunk_patch_unary(Chunk *c, Bookmark idx, Opcode op, uint64_t operand) {
  chunk_patch_operation(c, idx, op);
  chunk_patch_operand(c, idx + 1, operand);
}

size_t chunk_record_const(Chunk *c, Val v) {
  size_t idx = 0;
  if (list_find(&c->consts, v, &idx)) {
    return idx;
  }
  list_append(&c->consts, v);
  return list_len(&c->consts);
}

void chunk_seal(Chunk *c) {
  arraylist_seal(Opcode)(&c->bytecode);
  arraylist_seal(Line)(&c->lines);
  arraylist_seal(Val)(&c->consts.al);
}

void chunk_free(Chunk *c) {
  arraylist_free(Opcode)(&c->bytecode);
  arraylist_free(Line)(&c->lines);
  list_free(&c->consts);
}

#define OPCODE(STRING) #STRING,
static char const *opcodes[] = {
#include "bytecode_ops.incl"
};
#undef OPCODE

static size_t disassamble_op(const Chunk *chunk, size_t offset, Line l) {
  assert(offset < chunk_len(chunk));
  printf("%6zu ", offset);
  if (!l) { // line zero keeps the previous line number
    printf("     | ");
  } else {
    printf("%6zu ", l);
  }
  Opcode op = *arraylist_get(Opcode)(&chunk->bytecode, offset);
  offset++;
  if (op >= OP__MAX) {
    printf("bad opcode: %" PRIu8 "\n", op);
    return offset;
  }
  const char *name = opcodes[op];
  switch (op) {
  case OP_SET_GLOBAL:
  case OP_GET_GLOBAL:
  case OP_DEF_GLOBAL:
  case OP_CONSTANT: {
    uint64_t const_idx = chunk_read_operator(chunk, &offset);
    printf("%-16s %6" PRIu64 " ", name, const_idx);
    val_print(list_get(&chunk->consts, const_idx, VAL_NIL));
    printf("\n");
    break;
  }
  case OP_SET_LOCAL:
  case OP_GET_LOCAL:
  case OP_LOOP:
  case OP_JUMP:
  case OP_JUMP_IF_FALSE:
  case OP_JUMP_IF_TRUE: {
    uint64_t idx = chunk_read_operator(chunk, &offset);
    printf("%-16s %6" PRIu64 "\n", name, idx);
    break;
  }
  default:
    printf("%-16s\n", name);
    break;
  }
  return offset;
}

void chunk_disassamble(const Chunk *c) {
  size_t last_line = SIZE_MAX;
  size_t line = 0;
  size_t offset = 0;
  while (offset < chunk_len(c)) {
    for (; offset >= *arraylist_get(Line)(&c->lines, line); line++) {
    }
    offset = disassamble_op(c, offset, last_line != line ? line : 0);
    last_line = line;
  }
}

extern inline size_t chunk_len(const Chunk *);
extern uint64_t chunk_read_operator(const Chunk *, size_t *);
