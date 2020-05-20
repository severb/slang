#include "mem.h"

#include <assert.h> // assert
#include <stdio.h>  // fputs, stderr
#include <stdlib.h> // free, realloc, abort

#include "safemath.h"

#ifdef SLANG_DEBUG
static MemStats stats;
#endif

void mem_error(const char *msg) {
  (void)msg;
#ifndef SLANG_COREDUMP
  fputs(msg, stderr);
  abort();
#endif
}

void *mem_reallocate(void *p, size_t old_size, size_t new_size) {
#ifdef SLANG_DEBUG
  stats.calls++;
#endif
  if (new_size == 0) {
#ifdef SLANG_DEBUG
    if (size_t_sub_over(stats.bytes, old_size, &stats.bytes)) {
      mem_error("freeing more bytes than allocated");
    }
#endif
    free(p);
    return 0;
  }
  if (new_size == old_size) {
    return p;
  }
#ifdef SLANG_DEBUG
  if (size_t_sub_over(stats.bytes, old_size, &stats.bytes)) {
    mem_error("freeing more bytes than allocated");
  }
  if (size_t_add_over(stats.bytes, new_size, &stats.bytes)) {
    mem_error("allocating more than SIZE_T_MAX");
  }
#endif
  void *res = realloc(p, new_size);
  if (!res) {
    mem_error("out of memory");
  }
  return res;
}

extern inline void *mem_allocate(size_t);
extern inline void mem_free(void *, size_t);
extern inline void *mem_resize_array(void *, size_t item_size, size_t old_len,
                                     size_t new_len);
extern inline void mem_free_array(void *, size_t item_size, size_t old_len);

extern inline void *mem_allocate_flex(size_t type_size, size_t item_size,
                                      size_t len);
extern inline void *mem_resize_flex(void *, size_t type_size, size_t item_size,
                                    size_t old_len, size_t new_len);
extern inline void mem_free_flex(void *, size_t type_size, size_t item_size,
                                 size_t len);
