#include "bytecode.h"

#include "table.h"

#define OPCODE(STRING) #STRING,
static char const *OPCODE_TO_STRING[] = {
#include "opcodes.enum"
};
#undef OPCODE

Chunk *chunk_init(Chunk *chunk) {
  if (chunk == 0) {
    return 0;
  }
  *chunk = (Chunk){0};
  table_init(&chunk->const_to_idx);
  return chunk;
}

void chunk_destroy(Chunk *chunk) {
  array_destroy(&chunk->consts);
  table_destroy(&chunk->const_pos);
  FREE_ARRAY(chunk->code, uint8_t, chunk->cap);
  FREE_ARRAY(chunk->lines, size_t, chunk->cap);
  chunk_init(chunk);
}

void chunk_write(Chunk *chunk, uint8_t byte, size_t line) {
  if (chunk->cap < chunk->len + 1) {
    size_t old_cap = chunk->cap;
    chunk->cap = GROW_CAPACITY(old_cap);
    chunk->code = GROW_ARRAY(chunk->code, uint8_t, old_cap, chunk->cap);
    chunk->lines = GROW_ARRAY(chunk->lines, size_t, old_cap, chunk->cap);
  }
  chunk->code[chunk->len] = byte;
  chunk->lines[chunk->len] = line;
  chunk->len++;
}

void chunk_write2(Chunk *chunk, uint8_t bytes[2], size_t line) {
  chunk_write(chunk, bytes[0], line);
  chunk_write(chunk, bytes[1], line);
}

void chunk_write3(Chunk *chunk, uint8_t bytes[3], size_t line) {
  chunk_write(chunk, bytes[0], line);
  chunk_write(chunk, bytes[1], line);
  chunk_write(chunk, bytes[2], line);
}

static size_t chunk_add_const(Chunk *chunk, Val *constant) {
  Val *pos = 0;
  if ((pos = table_get(&chunk->const_pos, *constant))) {
    val_destroy(constant);
    assert(VAL_IS_NUMBER(*pos));
    return (uint16_t)pos->val.as.number;
  }
  Val copy = *constant;
  if (VAL_IS_STR(copy))
    copy = VAL_LIT_SLICE(str_slice(copy.val.as.str));
  uint16_t const_pos = array_append(&chunk->consts, constant);
  Val val = VAL_LIT_NUMBER(const_pos);
  table_set(&chunk->const_pos, &copy, &val);
  return const_pos;
}

void chunk_emit_idx(Chunk *chunk, OpCode op, uint16_t idx, size_t line) {
  chunk_write(chunk, op + (idx > UINT8_MAX), line);
  if (idx <= UINT8_MAX)
    chunk_write(chunk, idx, line);
  else
    chunk_write2(chunk, (uint8_t[]){idx >> 8, (uint8_t)idx}, line);
}

bool chunk_emit_const(Chunk *chunk, OpCode op, Val *constant, size_t line) {
  size_t idx = chunk_add_const(chunk, constant);
  if (idx >= UINT16_MAX)
    return false;
  chunk_emit_idx(chunk, op, idx, line);
  return true;
}

void chunk_seal(Chunk *chunk) {
  table_destroy(&chunk->const_pos);
  array_seal(&chunk->consts);
  chunk->code = GROW_ARRAY(chunk->code, uint8_t, chunk->cap, chunk->len);
  chunk->lines = GROW_ARRAY(chunk->lines, size_t, chunk->cap, chunk->len);
  chunk->cap = chunk->len;
}

size_t chunk_print_dis_instr(const Chunk *chunk, size_t offset) {
  printf("%04zu ", offset);
  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1])
    printf("   | ");
  else
    printf("%4zu ", chunk->lines[offset]);
  uint8_t instr = chunk->code[offset];
  const char *name = OPCODE_TO_STRING[instr];
  switch (instr) {
  case OP_SET_GLOBAL:
  case OP_GET_GLOBAL:
  case OP_DEF_GLOBAL:
  case OP_CONSTANT: {
    uint8_t const_idx = chunk->code[offset + 1];
    printf("%-16s %4u ", name, const_idx);
    Val constant = chunk->consts.vals[const_idx];
    val_print_repr(constant);
    printf("\n");
    return offset + 2;
  }
  case OP_SET_GLOBAL2:
  case OP_GET_GLOBAL2:
  case OP_DEF_GLOBAL2:
  case OP_CONSTANT2: {
    uint16_t idx = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
    printf("%-16s %4u ", name, idx);
    Val constant = chunk->consts.vals[idx];
    val_print_repr(constant);
    printf("\n");
    return offset + 3;
  }
  case OP_SET_LOCAL:
  case OP_GET_LOCAL: {
    uint8_t const_idx = chunk->code[offset + 1];
    printf("%-16s %4u", name, const_idx);
    printf("\n");
    return offset + 2;
  }
  case OP_LOOP:
  case OP_JUMP:
  case OP_JUMP_IF_FALSE:
  case OP_JUMP_IF_TRUE:
  case OP_SET_LOCAL2:
  case OP_GET_LOCAL2: {
    uint16_t idx = (chunk->code[offset + 1] << 8) | chunk->code[offset + 2];
    printf("%-16s %4u", name, idx);
    printf("\n");
    return offset + 3;
  }
  default: {
    printf("%-16s\n", name);
    return offset + 1;
  }
  }
}

void chunk_print_dis(const Chunk *chunk) {
  for (size_t offset = 0; offset < chunk->len;) {
    offset = chunk_print_dis_instr(chunk, offset);
  }
}