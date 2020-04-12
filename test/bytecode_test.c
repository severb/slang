#include "bytecode.h"
#include "mem.h"

#include <stdlib.h> // EXIT_SUCCESS

int main(int argc, char *argv[]) {
  Chunk c = {.lines = {0}, .bytecode = {0}, .consts = {0}};
  chunk_write(&c, OP_NOOP, 1);
  chunk_write(&c, OP_NOOP, 1);
  chunk_write(&c, OP_NOOP, 1);
  chunk_write(&c, OP_NOOP, 1);

  size_t r1 = chunk_reserve(&c, 2);
  size_t r2 = chunk_reserve_unary(&c, 2);
  size_t r3 = chunk_reserve_unary(&c, 2);

  chunk_write(&c, OP_NOOP, 3);
  chunk_write(&c, OP_NOOP, 3);
  chunk_write(&c, OP_NOOP, 3);
  chunk_write(&c, OP_NOOP, 3);

  chunk_write_unary(&c, OP_LOOP, 9, 0x7f);
  chunk_write_unary(&c, OP_JUMP, 9, 0xf0);
  chunk_write_unary(&c, OP_JUMP_IF_FALSE, 11, 0xfff0);
  chunk_write_unary(&c, OP_JUMP_IF_TRUE, 11, 0xfffffffffffffff0);

  chunk_patch(&c, r1, OP_NEGATE);
  chunk_patch_unary(&c, r2, OP_DEF_GLOBAL, 0x7f);
  chunk_patch_unary(&c, r3, OP_DEF_GLOBAL, 0x7f7f);

  chunk_disassamble(&c);
  chunk_free(&c);
#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
