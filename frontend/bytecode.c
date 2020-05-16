#include "bytecode.h"

#include "dynarray.h" // dynarray_*
#include "list.h"     // list_*
#include "mem.h"      // mem_free
#include "tag.h"      // Tag, tag_*

#include <assert.h>   // assert
#include <inttypes.h> // PRI*
#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t, SIZE_MAX
#include <stdio.h>    // printf, putchar

static void write_byte(Chunk *c, size_t line, uint8_t op) {
  // TODO: out of memory checks
  dynarray_append(uint8_t)(&c->bytecode, &op);
  size_t len = dynarray_len(size_t)(&c->lines);
  size_t prev = len > 0 ? *dynarray_get(size_t)(&c->lines, len - 1) : 0;
  while (line >= dynarray_len(size_t)(&c->lines)) {
    dynarray_append(size_t)(&c->lines, &prev);
  }
  *dynarray_get(size_t)(&c->lines, dynarray_len(size_t)(&c->lines) - 1) += 1;
}

void chunk_write_operation(Chunk *c, size_t line, uint8_t op) {
  write_byte(c, line, op);
}

// least significant byte 1st, max 9 bytes, 9th byte not tagged
static void chunk_write_operand(Chunk *c, size_t line, uint64_t operand) {
  for (int i = 0; i < 8; i++) {
    if (operand < 0x80) {
      write_byte(c, line, operand);
      return;
    }
    write_byte(c, line, 0x80 | (operand & 0x7f));
    operand >>= 7;
  }
  write_byte(c, line, operand); // last full byte
}

void chunk_write_unary(Chunk *c, size_t line, uint8_t op, uint64_t operand) {
  chunk_write_operation(c, line, op);
  chunk_write_operand(c, line, operand);
}

size_t chunk_reserve_unary(Chunk *c, size_t line) {
  size_t idx = dynarray_len(uint8_t)(&c->bytecode);
  for (int i = 0; i < 10; i++) { // 1 op + 8 + 1 bytes max operand size
    chunk_write_operation(c, line, OP_NOOP);
  }
  return idx;
}

void chunk_patch_unary(Chunk *c, size_t bookmark, uint8_t op) {
  size_t clen = chunk_len(c);
  assert(bookmark <= clen && "invalid bookmark");
  uint64_t oper = clen - bookmark;
  assert(oper >= 10 && "invalid bookmark");
  oper -= 10;
  *dynarray_get(uint8_t)(&c->bytecode, bookmark) = op;
  for (int i = 1; i < 9; i++) {
    *dynarray_get(uint8_t)(&c->bytecode, bookmark + i) = 0x80 | (0x7f & oper);
    oper >>= 7;
  }
  *dynarray_get(uint8_t)(&c->bytecode, bookmark + 9) = oper;
}

size_t chunk_record_const(Chunk *c, Tag t) {
  size_t idx = 0;
  if (list_find_from(&c->consts, t, &idx)) {
    // prevent converting float literals to int literals
    if (tag_type(t) == tag_type(*list_get(&c->consts, idx))) {
      return idx;
    }
    idx++;
  }
  // TODO: out of memory check
  return list_append(&c->consts, t) - 1;
}

void chunk_seal(Chunk *c) {
  dynarray_seal(uint8_t)(&c->bytecode);
  dynarray_seal(size_t)(&c->lines);
  // list_seal(&c->consts); TODO: seal?
}

void chunk_destroy(Chunk *c) {
  dynarray_destroy(uint8_t)(&c->bytecode);
  dynarray_destroy(size_t)(&c->lines);
  list_destroy(&c->consts);
}

void chunk_free(Chunk *c) {
  chunk_destroy(c);
  mem_free(c, sizeof(Chunk));
}

#define OPCODE(STRING) #STRING,
static char const *opcodes[] = {
#include "bytecode_ops.incl"
};
#undef OPCODE

static size_t disassamble_op(const Chunk *chunk, size_t offset, size_t line) {
  assert(offset < chunk_len(chunk));
  printf("%6zu ", offset);
  if (!line) { // line zero keeps the previous line number
    printf("     | ");
  } else {
    printf("%6zu ", line);
  }
  uint8_t op = *dynarray_get(uint8_t)(&chunk->bytecode, offset);
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
  case OP_GET_CONSTANT: {
    uint64_t const_idx = chunk_read_operator(chunk, &offset);
    printf("%-16s %6" PRIu64 " (", name, const_idx);
    tag_repr(*list_get(&chunk->consts, const_idx));
    printf(")\n");
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

size_t chunk_lines_delta(const Chunk *c, size_t l, size_t new_offset) {
  size_t offset = *dynarray_get(size_t)(&c->lines, l);
  if (offset > new_offset) {
    return 0;
  }
  size_t delta = 0;
  while (offset >= *dynarray_get(size_t)(&c->lines, l + delta)) {
    delta++;
  }
  return delta;
}

void chunk_disassamble(const Chunk *c) {
  size_t last_line = SIZE_MAX;
  size_t line = 1;
  size_t offset = 0;
  while (offset < chunk_len(c)) {
    line += chunk_lines_delta(c, line - 1, offset);
    offset = disassamble_op(c, offset, last_line != line ? line : 0);
    last_line = line;
  }
}

static const char *skip_lines(const char *src, size_t lines) {
  for (; lines > 1; lines--) {
    for (; *src != '\n' && *src != '\0'; src++) {
    };
    if (*src == '\n') {
      src++;
    }
  }
  return src;
}

void chunk_disassamble_src(const Chunk *c,const char *src) {
  printf("constants: ");
  list_print(&c->consts);
  printf("\n");
  size_t printed_lines = 0;
  size_t line = 1;
  size_t offset = 0;
  while (offset < chunk_len(c)) {
    line += chunk_lines_delta(c, line - 1, offset);
    if (printed_lines < line) {
      // skip empty lines
      src = skip_lines(src, line - printed_lines);
      printf("\n");
      printf("%13zu ", line);
      if (*src != '\0') {
        while (*src != '\n' && *src != '\0') {
          putchar(*src);
          src++;
        }
        putchar('\n');
        if (*src != '\0') {
          src++;
        }
      } else {
        printf("at end of file\n");
      }
      printed_lines = line;
    }
    offset = disassamble_op(c, offset, 0);
  }
}

extern inline size_t chunk_len(const Chunk *);
extern uint8_t chunk_read_opcode(const Chunk *, size_t);
extern uint64_t chunk_read_operator(const Chunk *, size_t *);
extern inline Tag chunk_get_const(const Chunk *, size_t);
