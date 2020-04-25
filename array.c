#include "array.h"

#include "mem.h" // mem_resize_array, mem_free_array

#include <stdbool.h> // bool
#include <stdio.h>   // fputs, stderr
#include <stdlib.h>  // abort

static size_t pow2(size_t n) {
  size_t pow = 1;
  while (pow < n && pow) {
    pow <<= 1;
  }
  return pow;
}

static bool is_pow2(size_t n) { return (n & (n - 1)) == 0; }

void gal_reserve(GenericArrayList *gal, size_t item_size, size_t cap) {
  if (gal->cap >= cap) {
    return;
  }
  size_t pow_cap = cap;
  if (pow_cap < 8) {
    pow_cap = 8;
  }
  if (!is_pow2(pow_cap)) {
    pow_cap = pow2(pow_cap);
  }
  if (!pow_cap) {
    goto out_of_memory;
  }
  if (SIZE_MAX / item_size < pow_cap) {
    goto out_of_memory;
  }
  gal->items = mem_resize_array(gal->items, item_size, gal->cap, pow_cap);
  if (!gal->items) {
    *gal = (GenericArrayList){0};
    goto out_of_memory;
  }
  gal->cap = pow_cap;
  return;
out_of_memory:
  fputs("out of memory when growing sequence", stderr);
  abort();
}

void gal_grow(GenericArrayList *gal, size_t item_size) {
  if (gal->cap <= SIZE_MAX / 2) {
    gal_reserve(gal, item_size, gal->cap ? gal->cap << 1 : 1);
  } else {
    fputs("out of memory when growing sequence", stderr);
    abort();
  }
}

void gal_free(GenericArrayList *gal, size_t item_size) {
  mem_free_array(gal->items, item_size, gal->cap);
  *gal = (GenericArrayList){0};
}

void gal_seal(GenericArrayList *gal, size_t item_size) {
  mem_resize_array(gal->items, item_size, gal->cap, gal->len);
  gal->cap = gal->len;
}
