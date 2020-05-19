#include "mem.h"

#include <assert.h> // assert
#include <stdlib.h> // free, realloc, abort

#ifdef SLANG_NOCOREDUMP
#include <stdio.h> // fputs, stderr
#endif

#ifdef SLANG_DEBUG
static MemStats stats;
MemStats mem_stats(void) { return stats; }
#endif

void *mem_reallocate(void *p, size_t old_size, size_t new_size) {
#ifdef SLANG_DEBUG
  stats.calls++;
#endif
  if (new_size == 0) {
#ifdef SLANG_DEBUG
    assert(old_size <= stats.bytes && "freeing more than allocated");
    stats.bytes -= old_size;
#endif
    free(p);
    return 0;
  }
  if (new_size == old_size) {
    return p;
  }
#ifdef SLANG_DEBUG
  if (new_size > old_size) {
    size_t diff = new_size - old_size;
    assert(stats.bytes < SIZE_MAX - diff && "allocating more than SIZE_MAX");
    stats.bytes += diff;
  } else {
    size_t diff = old_size - new_size;
    assert(stats.bytes >= diff && "freeing more than allocated");
    stats.bytes -= diff;
  }
#endif
  void *res = realloc(p, new_size);
#ifdef SLANG_NOCOREDUMP
  if (!res) {
    fputs("out of memory", stderr);
    abort();
  }
#endif
  return res;
}

extern inline void *mem_allocate(size_t);
extern inline void mem_free(void *, size_t);
extern inline void *mem_resize_array(void *, size_t item_size, size_t old_len,
                                     size_t new_len);
extern inline void mem_free_array(void *, size_t item_size, size_t old_len);

extern inline void *mem_allocate_flex(size_t type_size, size_t item_size,
                                      size_t len);
extern inline void mem_free_flex(void *, size_t type_size, size_t item_size,
                                 size_t len);
