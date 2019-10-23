#ifndef clox_array_h
#define clox_array_h

#include "common.h"
#include "val.h"

typedef struct sArray {
  size_t cap;
  size_t len;
  Val *vals;
} Array;

Array *array_init(Array *);
void array_destroy(Array *);

size_t array_append(Array *, Val *);
void array_seal(Array *);
Val *array_get(const Array *, size_t i);
Val *array_pop(Array *);

#endif
