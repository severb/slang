#include "array.h"

#include "mem.h"

#include <stdio.h>  // fputs, stderr
#include <stdlib.h> // abort

void al_reserve(AL *a, size_t item_size, size_t cap) {
  if (a->cap >= cap) {
    return;
  }
  size_t pow_cap = cap;
  if (pow_cap < 8 || (pow_cap & (pow_cap - 1))) {
    pow_cap = 8;
    while (pow_cap < cap && pow_cap) {
      pow_cap <<= 1;
    }
  }
  if (!pow_cap) {
    goto out_of_memory;
  }
  if (SIZE_MAX / item_size < pow_cap) {
    goto out_of_memory;
  }
  a->items = mem_resize_array(a->items, item_size, a->cap, pow_cap);
  if (!a->items) {
    *a = (AL){0};
    goto out_of_memory;
  }
  a->cap = pow_cap;
  return;
out_of_memory:
  fputs("out of memory when growing sequence", stderr);
  abort();
}

void al_grow(AL *a, size_t item_size) {
  size_t new_cap;
  if (a->cap < 8) {
    new_cap = 8;
  } else if (a->cap < SIZE_MAX) {
    new_cap = a->cap + 1;
  } else {
    goto out_of_memory;
  }
  al_reserve(a, item_size, new_cap);
  return;
out_of_memory:
  fputs("out of memory when growing sequence", stderr);
  abort();
}

void al_free(AL *a, size_t item_size) {
  mem_free_array(a->items, item_size, a->cap);
  *a = (AL){0};
}

void al_seal(AL *a, size_t item_size) {
  mem_resize_array(a->items, item_size, a->cap, a->len);
  a->cap = a->len;
}
