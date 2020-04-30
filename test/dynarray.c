#include "types/dynarray.h" // arraylist_*
#include "mem.h"            // mem_allocation_summary

#include <assert.h> // assert
#include <stddef.h> // size_t
#include <stdio.h>  // printf
#include <stdlib.h> // EXIT_SUCCESS

int main(void) {
  DynamicArray(size_t) a = {0};
  size_t x = 10;
  assert(dynarray_append(size_t)(&a, &x) && "cannot add to dynarray");
  x = 20;
  assert(dynarray_append(size_t)(&a, &x) && "cannot add to dynarray");
  x = 30;
  assert(dynarray_append(size_t)(&a, &x) && "cannot add to dynarray");
  printf("%zu %zu %zu\n", *dynarray_get(size_t)(&a, 0),
         *dynarray_get(size_t)(&a, 1), *dynarray_get(size_t)(&a, 2));
  printf("len: %zu\n", dynarray_len(size_t)(&a));
  printf("cap: %zu\n", dynarray_cap(size_t)(&a));
  dynarray_free(size_t)(&a);

#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
