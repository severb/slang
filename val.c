#include "val.h"

#include "array.h"
#include "common.h"
#include "str.h"
#include "table.h"

#include <stdio.h>

void val_destroy(Val *val) {
  if (val == 0)
    return;
  switch (val->type) {
  case VAL_ARRAY:
    array_destroy(val->as.array);
    break;
  case VAL_STR:
    str_free(val->as.str);
    break;
  case VAL_TABLE:
    table_destroy(val->as.table);
    break;
  default: // ignore the others
    break;
  }
  *val = LIT_VAL_NIL;
}

void val_print(Val const *val) {
  switch (val->type) {
  case VAL_ARRAY:
    printf("<array>");
    break;
  case VAL_BOOL:
    printf(val->as.boolean ? "true" : "false");
    break;
  case VAL_NIL:
    printf("nil");
    break;
  case VAL_NUMBER:
    printf("%g", val->as.number);
    break;
  case VAL_SLICE:
    slice_print(val->as.slice);
    break;
  case VAL_STR:
    slice_print(str_slice(val->as.str));
    break;
  case VAL_TABLE:
    printf("<table>");
    break;
  default:
    assert(0);
  }
}

#define GENERATE_STRING(STRING) #STRING,
static const char *VAL_NAMES[] = {FOREACH_VALTYPE(GENERATE_STRING)};
#undef GENERATE_STRING

void val_print_repr(Val const *val) {
  const char *name = VAL_NAMES[val->type];
  switch (val->type) {
  case VAL_ARRAY:
    printf("<%s[%zu/%zu] @%p>", name, val->as.array->len, val->as.array->cap,
           (void *)val->as.array);
  case VAL_BOOL:
  case VAL_NUMBER:
    printf("<%s ", name);
    val_print(val);
    printf(">");
    break;
  case VAL_SLICE:
    printf("<%s[%u] @%p '", name, val->as.slice.len, (void *)val->as.slice.c);
    slice_print(val->as.slice);
    printf("'>");
  case VAL_STR:
    printf("<%s[%u] @%p '", name, val->as.str->len, (void *)val->as.str);
    slice_print(str_slice(val->as.str));
    printf("'>");
    break;
  case VAL_TABLE:
    printf("<%s[%zu/%zu] @%p>", name, val->as.table->len, val->as.table->cap,
           (void *)val->as.table);
  default:
    printf("<%s>", name);
    break;
  }
}

uint32_t val_hash(Val const *val) {
  void *p = 0;
  switch (val->type) {
  case VAL_ARRAY:
    p = val->as.array;
    goto ptr;
  case VAL_BOOL:
    return val->as.boolean ? 7 : 77;
  case VAL_NIL:
    return 777;
  case VAL_NUMBER: {
    union {
      double d;
      char c[sizeof(double)];
    } x = {.d = val->as.number};
    return str_hash(x.c, sizeof(double));
  }
  case VAL_SLICE:
    return val->as.slice.hash;
  case VAL_STR:
    return val->as.str->hash;
  case VAL_TABLE: {
    p = val->as.array;
    goto ptr;
  }
  default:
    assert(0);
  }
ptr:
  // discard the last 3 bits which are likely unset because of alignment
  return (uint32_t)((intptr_t)p >> 3);
}

bool val_truthy(Val const *val) {
  if (IS_VAL_BOOL(*val))
    return val->as.boolean;
  if (IS_VAL_NIL(*val))
    return false;
  return true;
}

bool val_equals(Val const *a, Val const *b) {
  bool a_slice = IS_VAL_SLICE(*a);
  bool b_slice = IS_VAL_SLICE(*b);
  if (a_slice && IS_VAL_STR(*b))
    return slice_equals(a->as.slice, str_slice(b->as.str));
  if (b_slice && IS_VAL_STR(*a))
    return slice_equals(str_slice(a->as.str), b->as.slice);
  if (a->type != b->type)
    return false;
  switch (a->type) {
  case VAL_ARRAY: // identity
    return a->as.array == b->as.array;
  case VAL_BOOL:
    return a->as.boolean == b->as.boolean;
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return a->as.number == b->as.number;
  case VAL_SLICE:
    return slice_equals(a->as.slice, b->as.slice);
  case VAL_STR:
    return slice_equals(str_slice(a->as.str), str_slice(b->as.str));
  case VAL_TABLE: // identity
    return a->as.table == b->as.table;
  default:
    assert(0);
  }
}
