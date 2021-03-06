#include "dynarray.h"

#include "mem.h"      // mem_*
#include "safemath.h" // size_t_mul_over

#include <stddef.h>
#include <stdint.h>

#define MIN_CAP 8

static size_t next_pow2(size_t n) {
    if ((n & (n - 1)) == 0) {
        return n;
    }
    size_t pow = 1;
    while (pow && pow < n) {
        pow <<= 1;
    }
    return pow;
}

void dynarray_reserve_T(DynamicArrayT *array, size_t cap, size_t item_size) {
    if (array->cap >= cap) {
        return;
    }
    if (cap < MIN_CAP) {
        cap = MIN_CAP;
    } else {
        cap = next_pow2(cap);
    }
    if (!cap) {
        mem_error("dynamic array reserve size too large");
        // if mem_error didn't abort(), force a core dump on next access
        *array = (DynamicArrayT){0};
        return;
    }
    array->items = mem_resize_array(array->items, item_size, array->cap, cap);
    array->cap = cap;
}

void dynarray_grow_T(DynamicArrayT *array, size_t item_size) {
    size_t new_cap = MIN_CAP;
    if (array->cap && size_t_mul_over(array->cap, 2, &new_cap)) {
        mem_error("dynamic array grow size too large");
        // if mem_error didn't abort(), force a core dump on next access
        *array = (DynamicArrayT){0};
        return;
    }
    dynarray_reserve_T(array, new_cap, item_size);
}

void dynarray_destroy_T(DynamicArrayT *array, size_t item_size) {
    mem_free_array(array->items, item_size, array->cap);
    *array = (DynamicArrayT){0};
}

void dynarray_free_T(DynamicArrayT *array, size_t item_size) {
    dynarray_destroy_T(array, item_size);
    mem_free(array, sizeof(*array));
}

void dynarray_seal_T(DynamicArrayT *array, size_t item_size) {
    array->items = mem_resize_array(array->items, item_size, array->cap, array->len);
}

// predefined dynamic lists
dynarray_define(size_t);
dynarray_define(uint8_t);
