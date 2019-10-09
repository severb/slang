#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "mem.h"

size_t mem_allocated;
size_t mem_freed;

void *reallocate(void *previous, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    mem_freed += old_size;
    free(previous);
    return NULL;
  }
  if (old_size < new_size)
    mem_allocated += new_size - old_size;
  else if (old_size > new_size)
    mem_freed += old_size - new_size;
  void *result = realloc(previous, new_size);
  return result;
}
