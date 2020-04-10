#include "bytecode.h"

#include "array.h" // arraylist_*
#include "types.h" // list_*
#include "val.h"   // Val, val_eq

#include <assert.h> // assert
#include <stddef.h> // size_t
#include <stdint.h> // uint*_t, SIZE_MAX

arraylist_define(Opcode);
arraylist_define(Line);

static void write_byte(Chunk *c, uint8_t b, Line l) {
  arraylist_append(Opcode)(&c->bytecode, &b);
  while (l >= arraylist_len(Line)(&c->lines)) {
    Line zero = 0;
    arraylist_append(Line)(&c->lines, &zero);
  }
  *arraylist_get(Line)(&c->lines, arraylist_len(Line)(&c->lines) - 1) += 1;
}

// least significant byte 1st, max 9 bytes, 9th byte not tagged
static void write_operand(Chunk *c, uint64_t operand, Line l) {
  for (int i = 0; i < 8; i++) {
    if (operand < 0x80) {
      write_byte(c, operand, l);
      return;
    }
    write_byte(c, 0x80 | (operand & 0x7f), l);
    operand >>= 7;
  }
  write_byte(c, operand, l); // last full byte
}

void chunk_write(Chunk *c, Opcode op, Line l) { write_byte(c, op, l); }

void chunk_write_unary(Chunk *c, Opcode op, Line l, uint64_t operand) {
  write_byte(c, op, l);
  write_operand(c, operand, l);
}

void chunk_write_binary(Chunk *c, Opcode op, Line l, uint64_t o1, uint64_t o2) {
  write_byte(c, op, l);
  write_operand(c, o1, l);
  write_operand(c, o2, l);
}

size_t chunk_reserve(Chunk *c, Line l) {
  size_t idx = arraylist_len(Opcode)(&c->bytecode);
  chunk_write(c, OP_NOOP, l);
  return idx;
}

size_t chunk_reserve_unary(Chunk *c, Line l) {
  size_t idx = arraylist_len(Opcode)(&c->bytecode);
  for (int i = 0; i < 10; i++) { // 1 byte opcode, 8 + 1 bytes max oper
    chunk_write(c, OP_NOOP, l);
  }
  return idx;
}

size_t chunk_reserve_binary(Chunk *c, Line l) {
  size_t idx = arraylist_len(Opcode)(&c->bytecode);
  for (int i = 0; i < 19; i++) { // 1 byte opcode, 2 * (8 + 1 bytes max oper)
    chunk_write(c, OP_NOOP, l);
  }
  return idx;
}

static void patch_byte(Chunk *c, size_t idx, uint8_t b) {
  assert(idx < arraylist_len(Opcode)(&c->bytecode));
  *arraylist_get(Opcode)(&c->bytecode, idx) = b;
}

static void patch_operand(Chunk *c, size_t idx, uint64_t operand) {
  assert(idx < SIZE_MAX - 9);
  for (int i = 0; i < 8; i++) {
    patch_byte(c, idx + i, 0x80 | (0x7F & operand));
    operand >>= 7;
  }
  patch_byte(c, idx + 8, operand);
}

void chunk_patch(Chunk *c, size_t idx, Opcode op) { patch_byte(c, idx, op); }

void chunk_patch_unary(Chunk *c, size_t idx, Opcode op, uint64_t operand) {
  patch_byte(c, idx, op);
  patch_operand(c, idx + 1, operand);
}

void chunk_patch_binary(Chunk *c, size_t idx, Opcode op, uint64_t o1,
                        uint64_t o2) {
  patch_byte(c, idx, op);
  patch_operand(c, idx + 1, o1);
  patch_operand(c, idx + 10, o2); // 1 byte opcode, 8 + 1 bytes max oper
}

#define SENTINEL USR_SYMBOL(0)

size_t chunk_record_const(Chunk *c, Val v) {
  for (size_t i = 0; i < list_len(&c->consts); i++) {
    if (val_eq(list_get(&c->consts, i, SENTINEL), v)) {
      return i;
    }
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

void chunk_disassamble(const Chunk *c) {
  for (size_t i = 0; i < arraylist_len(Opcode)(&c->bytecode); i++) {
    printf("%x\n", *arraylist_get(Opcode)(&c->bytecode, i));
  }
}

extern inline size_t chunk_position(const Chunk *);
