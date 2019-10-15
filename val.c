#include <stdio.h>

#include "mem.h"
#include "str.h"
#include "val.h"

#define GENERATE_STRING(STRING) #STRING,
static const char *VALTYPE_TO_STRING[] = {FOREACH_VALTYPE(GENERATE_STRING)};
#undef GENERATE_STRING

Val *val_init(Val *val) {
  if (val == 0)
    return 0;
  *val = VAL_LIT_NIL;
  return val;
}

void val_destroy(Val *val) {
  if (val == 0)
    return;
  if (VAL_IS_STR(*val))
    str_free(val->val.as.str);
  *val = VAL_LIT_NIL;
}

void val_print(Val val) {
  if (VAL_IS_SLICE(val))
    slice_print(val.slice);
  else
    switch (val.val.type) {
    case VAL_BOOL:
      printf(val.val.as.boolean ? "true" : "false");
      break;
    case VAL_NIL:
      printf("nil");
      break;
    case VAL_NUMBER:
      printf("%g", val.val.as.number);
      break;
    case VAL_STR:
      slice_print(str_slice(val.val.as.str));
      break;
    default:
      assert(0);
    }
}

void val_print_repr(Val val) {
  if (VAL_IS_SLICE(val)) {
    printf("<SLICE @%p '", val.slice.c);
    slice_print(val.slice);
    printf("'>");
  } else {
    const char *name = VALTYPE_TO_STRING[val.val.type];
    switch (val.val.type) {
    case VAL_STR:
      printf("<%s @%p '", name, (void *)val.val.as.str);
      slice_print(str_slice(val.val.as.str));
      printf("'>");
      break;
    case VAL_NUMBER:
    case VAL_BOOL:
      printf("<%s ", name);
      val_print(val);
      printf(">");
      break;
    default:
      printf("<%s>", name);
      break;
    }
  }
}

uint32_t val_hash(Val val) {
  if (VAL_IS_SLICE(val))
    return val.slice.hash;
  switch (val.val.type) {
  case VAL_BOOL:
    return val.val.as.boolean ? 7 : 77;
  case VAL_NIL:
    return 777;
  case VAL_NUMBER: {
    union {
      double d;
      char c[sizeof(double)];
    } x = {.d = val.val.as.number};
    return str_hash(x.c, sizeof(double));
  }
  case VAL_STR:
    return val.val.as.str->hash;
  default:
    assert(0);
  }
}

bool val_truthy(Val val) {
  if (VAL_IS_BOOL(val))
    return val.val.as.boolean;
  if (VAL_IS_NIL(val))
    return false;
  return true;
}

bool val_equals(Val a, Val b) {
  bool a_slice = VAL_IS_SLICE(a);
  bool b_slice = VAL_IS_SLICE(b);
  if (a_slice && b_slice)
    return slice_equals(a.slice, b.slice);
  if (a_slice && VAL_IS_STR(b))
    return slice_equals(a.slice, str_slice(b.val.as.str));
  if (b_slice && VAL_IS_STR(a))
    return slice_equals(str_slice(a.val.as.str), b.slice);
  if (a.val.type != b.val.type)
    return false;
  switch (a.val.type) {
  case VAL_BOOL:
    return a.val.as.boolean == b.val.as.boolean;
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return a.val.as.number == b.val.as.number;
  case VAL_STR:
    return slice_equals(str_slice(a.val.as.str), str_slice(b.val.as.str));
  default:
    assert(0);
  }
}
