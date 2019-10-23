#include "mem.h"

#include <assert.h> //assert
#include <stdlib.h> // free, realloc

// allocated_memory keeps track of how much memory is currently allocated.
// Assuming a bug-free implemenation, it can't wrap-around.
size_t allocated_memory;

void *reallocate(void *prev_ptr, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    assert(old_size < allocated_memory);
    allocated_memory -= old_size;
    free(prev_ptr);
    return 0;
  }
  if (new_size == old_size)
    return prev_ptr;
  if (new_size > old_size) {
    allocated_memory += new_size - old_size;
  } else {
    allocated_memory -= old_size - new_size;
  }
  return realloc(prev_ptr, new_size);
}
