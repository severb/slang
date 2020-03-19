#include "mem.h"

#include <assert.h> //assert
#include <stdio.h>  // printf
#include <stdlib.h> // free, realloc

static size_t allocated_memory;

void mem_allocation_summary(void) {
  printf("allocated memory: %zu\n", allocated_memory);
}

void *mem_reallocate(void *p, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    assert(old_size <= allocated_memory);
    allocated_memory -= old_size;
    free(p);
    return 0;
  }
  if (new_size == old_size)
    return p;
  if (new_size > old_size) {
    size_t diff = new_size - old_size;
    assert(allocated_memory < SIZE_MAX - diff);
    allocated_memory += diff;
  } else {
    size_t diff = old_size - new_size;
    assert(allocated_memory >= diff);
    allocated_memory -= diff;
  }
  void *res = realloc(p, new_size);
#ifdef MEMDEBUG
  mem_allocation_summary();
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
