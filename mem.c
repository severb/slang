#include "mem.h"

#ifdef MEMDEBUG
#include <stdio.h> // printf
#endif

#include <assert.h> //assert
#include <stdlib.h> // free, realloc

// Assuming a bug-free implemenation, allocated_memory can't wrap-around.
size_t allocated_memory;

static void *r(void *prev_ptr, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    assert(old_size <= allocated_memory);
    allocated_memory -= old_size;
    free(prev_ptr);
    return 0;
  }
  if (new_size == old_size)
    return prev_ptr;
  if (new_size > old_size) {
    allocated_memory += new_size - old_size;
  } else {
    assert(old_size - new_size <= allocated_memory);
    allocated_memory -= old_size - new_size;
  }
  return realloc(prev_ptr, new_size);
}

void *reallocate(void *prev_ptr, size_t old_size, size_t new_size) {
  void *res = r(prev_ptr, old_size, new_size);
#ifdef MEMDEBUG
  printf("used memory: %zu\n", allocated_memory);
#endif
  return res;
}
