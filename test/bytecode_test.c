#include "bytecode.h"
#include "mem.h"

#include <stdlib.h> // EXIT_SUCCESS

int main(int argc, char *argv[]) {
  Chunk c = {.lines = {0}, .bytecode = {0}, .consts = {0}};
  chunk_write(&c, 0xaa, 0);
  chunk_write(&c, 0xbb, 0);
  chunk_write(&c, 0xcc, 0);
  chunk_write(&c, 0xdd, 0);

  size_t r1 = chunk_reserve(&c, 0);
  size_t r2 = chunk_reserve_unary(&c, 0);
  size_t r3 = chunk_reserve_binary(&c, 0);

  chunk_write(&c, 0xaa, 0);
  chunk_write(&c, 0xbb, 0);
  chunk_write(&c, 0xcc, 0);
  chunk_write(&c, 0xdd, 0);

  chunk_write_unary(&c, 0xa1, 0, 0x7f);
  chunk_write_unary(&c, 0xa2, 0, 0xf0);
  chunk_write_unary(&c, 0xa3, 0, 0xfff0);
  chunk_write_unary(&c, 0xa4, 0, 0xfffffffffffffff0);

  chunk_write_binary(&c, 0xb1, 0, 0x7f, 0x7f);
  chunk_write_binary(&c, 0xb1, 0, 0xf0, 0xf0);

  chunk_patch(&c, r1, 0x11);
  chunk_patch_unary(&c, r2, 0x22, 0x7f);
  chunk_patch_binary(&c, r3, 0x33, 0x7f, 0x7f);

  chunk_disassamble(&c);
  chunk_free(&c);
#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
