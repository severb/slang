#include "types/dynarray.h"

#include "mem.h" // mem_resize_array, mem_free_array

#include <stddef.h> // size_t

static size_t next_pow2(size_t n) {
  if ((n & (n - 1)) == 0) {
    return n;
  }
  size_t pow = 1;
  while (pow < n && pow) {
    pow <<= 1;
  }
  return pow;
}

size_t dynarray_reserve_T(DynamicArrayT *array, size_t cap, size_t item_size) {
  if (array->cap >= cap) {
    return array->cap;
  }
  if (cap < 8) {
    cap = 8;
  } else {
    cap = next_pow2(cap);
  }
  if (!cap) {
    return 0;
  }
  array->items = mem_resize_array(array->items, item_size, array->cap, cap);
  if (!array->items) {
    *array = (DynamicArrayT){0};
    return 0;
  }
  array->cap = cap;
  return cap;
}

size_t dynarray_grow_T(DynamicArrayT *array, size_t item_size) {
  if (array->cap < SIZE_MAX) {
    return dynarray_reserve_T(array, 2 * array->cap, item_size);
  }
  return 0;
}

void dynarray_free_T(DynamicArrayT *array, size_t item_size) {
  mem_free_array(array->items, item_size, array->cap);
  *array = (DynamicArrayT){0};
}

size_t dynarray_seal_T(DynamicArrayT *array, size_t item_size) {
  mem_resize_array(array->items, item_size, array->cap, array->len);
  return array->cap = array->len;
}

// predefined dynamic lists
dynarray_define(size_t);
dynarray_define(uint8_t);
