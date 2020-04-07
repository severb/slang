#include "array.h"

#include "mem.h"

#include <stdio.h>  // fputs, stderr
#include <stdlib.h> // abort

void al_grow(AL *a, size_t item_size) {
  size_t oldcap = a->cap;
  if (oldcap < 8) {
    a->cap = 8;
  } else if (oldcap < (SIZE_MAX / 2 / item_size)) {
    a->cap = oldcap * 2;
  } else {
    goto out_of_memory;
  }
  a->items = mem_resize_array(a->items, item_size, oldcap, a->cap);
  if (a->items == 0) {
    goto out_of_memory;
  }
  return;
out_of_memory:
  fputs("out of memory when growing sequence", stderr);
  abort();
}

void al_free(AL *a, size_t item_size) {
  mem_free_array(a->items, item_size, a->cap);
  *a = (AL){0};
}
