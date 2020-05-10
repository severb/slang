#include "dynarray.h" // arraylist_*
#include "mem.h"      // mem_allocation_summary

#include <assert.h> // assert
#include <stddef.h> // size_t
#include <stdio.h>  // printf
#include <stdlib.h> // EXIT_SUCCESS

int main(void) {
  DynamicArray(size_t) a = {0};

  size_t x;
  for (size_t i = 0; i < 36; i++) {
    x = i;
    assert(dynarray_append(size_t)(&a, &x) && "cannot add to dynarray");
  }
  assert(*dynarray_get(size_t)(&a, 0) == 0 && "unexpected value");
  assert(*dynarray_get(size_t)(&a, 1) == 1 && "unexpected value");
  assert(*dynarray_get(size_t)(&a, 34) == 34 && "unexpected value");
  assert(*dynarray_get(size_t)(&a, 35) == 35 && "unexpected value");
  assert(dynarray_len(size_t)(&a) == 36 && "length doesn't match");
  assert(dynarray_cap(size_t)(&a) == 64 && "cap doesn't match");
  dynarray_free(size_t)(&a);

#ifdef SLANG_DEBUG
  assert(mem_stats().bytes == 0 && "unfreed memory");
#endif
  return EXIT_SUCCESS;
}
