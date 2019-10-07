#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "memory.h"

void *reallocate(void *previous, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
#ifdef DEBUG_PRINT_ALLOC
    printf("-- free --\t%p %5lu\n", previous, oldSize);
#endif
    free(previous);
    return NULL;
  }
  void *result = realloc(previous, newSize);
#ifdef DEBUG_PRINT_ALLOC
  if (oldSize == 0) {
    printf("-- alloc --\t%p %5lu\n", result, newSize);
  } else {
    printf("-- realloc --\t%p %5lu -> %p %5lu\n", previous, oldSize, result,
           newSize);
  }
#endif
  return result;
}
