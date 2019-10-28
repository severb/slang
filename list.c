#include "array.h"
#include "mem.h"
#include "val.h"

Array *array_init(Array *array) {
  if (array == 0)
    return 0;
  *array = (Array){0};
  return array;
}

void array_destroy(Array *array) {
  if (array == 0)
    return;
  for (int i = 0; i < array->len; i++) {
    val_destroy(&array->vals[i]);
  }
  FREE_ARRAY(array->vals, Val, array->cap);
  array_init(array);
}

size_t array_append(Array *array, Val *val) {
  if (array->cap < array->len + 1) {
    int old_cap = array->cap;
    array->cap = GROW_CAPACITY(old_cap);
    array->vals = GROW_ARRAY(array->vals, Val, old_cap, array->cap);
  }
  array->vals[array->len] = *val;
  *val = VAL_LIT_NIL;
  array->len++;
  return array->len - 1;
}

void array_seal(Array *array) {
  array->vals = GROW_ARRAY(array->vals, Val, array->cap, array->len);
  array->cap = array->len;
}

Val *array_get(const Array *array, size_t i) {
  if (!(0 <= i && i < array->len))
    return 0;
  return &array->vals[i];
}

Val *array_pop(Array *array) {
  if (array->len == 0)
    return 0;
  array->len--;
  return &array->vals[array->len];
}
