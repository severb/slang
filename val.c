#include "val.h"

#include "types.h" // String, string_free, string_print, string_hash
                   // Slice, slice_free, slice_print, slice_hash
                   // Table, table_free, table_print
                   // List, list_free, list_print

#include "mem.h" // mem_free, mem_free_flex

#include <inttypes.h> // PRId*
#include <stddef.h>   // max_align_t
#include <stdint.h>   // uint*_t, int*_t, uintptr_t
#include <stdio.h>    // printf
#include <string.h>   // memcmp
#include <stdbool.h>  // bool

void val_free(Val v) {
  if (!val_is_ptr_own(v))
    return; // we can only destroy owned pointer values
  switch (val_type(v)) {
  case VAL_STRING: {
    String *s = val_ptr2string(v);
    mem_free_flex(s, sizeof(String), sizeof(char), s->len);
    break;
  }
  case VAL_TABLE:
    table_free(val_ptr2table(v));
    mem_free(val_ptr2table(v), sizeof(Table));
    break;
  case VAL_LIST:
    list_free(val_ptr2list(v));
    mem_free(val_ptr2list(v), sizeof(List));
    break;
  case VAL_I64:
    mem_free(val_ptr2int64(v), sizeof(int64_t));
    break;
  case VAL_ERR:
    val_free(*val_ptr2err(v));
    mem_free(val_ptr2err(v), sizeof(Val));
    break;
  case VAL_SLICE:
    mem_free(val_ptr2slice(v), sizeof(Slice));
    break;
  case VAL_DOUBLE:
  case VAL_PAIR:
  case VAL_SYMBOL:
  default:
    assert(0);
  }
}

void val_print(Val v) {
  switch (val_type(v)) {
  case VAL_STRING: {
    string_print(val_ptr2string(v));
    break;
  }
  case VAL_TABLE:
    table_print(val_ptr2table(v));
    break;
  case VAL_LIST:
    list_print(val_ptr2list(v));
    break;
  case VAL_I64:
    printf("%" PRId64, *val_ptr2int64(v));
    break;
  case VAL_ERR:
    printf("error: ");
    val_print(*val_ptr2err(v));
    break;
  case VAL_SLICE: {
    slice_print(val_ptr2slice(v));
    break;
  }
  case VAL_PAIR:
    printf("(%" PRId16 ", %" PRId32 ")", val_data2pair_a(v),
           val_data2pair_b(v));
    break;
  case VAL_SYMBOL:
    switch (val_data2symbol(v)) {
    case SYM_FALSE:
      printf("false");
      break;
    case SYM_TRUE:
      printf("true");
      break;
    case SYM_NIL:
      printf("nil");
      break;
    case SYM_OK:
      printf("ok");
      break;
    default:
      printf("<symbol: %u>", val_data2symbol(v));
      break;
    }
    break;
  default:
    assert(0);
  }
}

union d {
  double d;
  uint64_t u;
};

size_t val_hash(Val v) {
  switch (val_type(v)) {
  case VAL_STRING:
    return string_hash(val_ptr2string(v));
  case VAL_TABLE:
  case VAL_LIST:
    return (size_t)0xDEADBEEF ^
           (size_t)((uintptr_t)val_ptr2ptr(v) >> sizeof(max_align_t));
  case VAL_I64:
    return (size_t)*val_ptr2int64(v);
  case VAL_ERR:
    return (size_t)0xC0FFEE ^ val_hash(*val_ptr2err(v));
  case VAL_SLICE:
    return slice_hash(val_ptr2slice(v));
  case VAL_DOUBLE:
    return (size_t)((union d){.d = val_data2double(v)}.u);
  case VAL_PAIR:
    return ((uint64_t)val_data2pair_ua(v) << 32 |
            (uint64_t)val_data2pair_ub(v));
  case VAL_SYMBOL:
    return (size_t)0xCACA0 ^ (size_t)val_data2symbol(v);
  default:
    assert(0);
  }
}

bool val_is_true(Val v) {
  switch (val_type(v)) {
  case VAL_STRING:
    return val_ptr2string(v)->len != 0;
  case VAL_TABLE:
    return val_ptr2table(v)->len != 0;
  case VAL_LIST:
    return val_ptr2list(v)->len != 0;
  case VAL_I64:
    return *val_ptr2int64(v) != 0;
  case VAL_ERR:
    return true;
  case VAL_SLICE:
    return val_ptr2slice(v)->len != 0;
  case VAL_DOUBLE:
    return val_data2double(v) != (double)0;
  case VAL_PAIR:
    return val_data2pair_ub(v) != 0 || val_data2pair_ua(v) != 0;
  case VAL_SYMBOL:
    switch (val_data2symbol(v)) {
    case SYM_FALSE:
    case SYM_NIL:
      return false;
    default:
      return true;
    }
  default:
    assert(0);
  }
}

bool val_eq(Val a, Val b) {
  size_t a_hash, a_len, b_hash, b_len;
  const char *a_c, *b_c;
  if (val_biteq(a, b)) {
    return true;
  }
  if (val_is_ptr(a) && (val_ptr2ptr(a) == val_ptr2ptr(b))) {
    return true;
  }
  switch (val_type(a)) {
  case VAL_STRING: {
    String *as = val_ptr2string(a);
    a_hash = as->hash;
    a_len = as->len;
    a_c = as->c;
    goto compare_strings;
  }
  case VAL_SLICE: {
    Slice *as = val_ptr2slice(a);
    a_hash = as->hash;
    a_len = as->len;
    a_c = as->c;
    goto compare_strings;
  }
  case VAL_I64:
    switch (val_type(b)) {
    case VAL_I64:
      return *val_ptr2int64(a) = *val_ptr2int64(b);
    case VAL_PAIR:
      return ((val_data2pair_ua(b) == 0) &&
              (*val_ptr2int64(a) == val_data2pair_b(b)));
    case VAL_DOUBLE:
      return *val_ptr2int64(a) == val_data2double(b);
    default:
      return false;
    }
  case VAL_DOUBLE:
    switch (val_type(b)) {
    case VAL_I64:
      return val_data2double(a) == *val_ptr2int64(b);
    case VAL_PAIR:
      return ((val_data2pair_ua(b) == 0) &&
              (val_data2double(a) == val_data2pair_b(b)));
    case VAL_DOUBLE:
      return val_data2double(a) == val_data2double(b);
    default:
      return false;
    }
  case VAL_ERR:
    return val_is_err(b) && val_eq(*val_ptr2err(a), *val_ptr2err(b));
  case VAL_TABLE:
    return (val_type(b) == VAL_TABLE) &&
           table_eq(val_ptr2table(a), val_ptr2table(b));
  case VAL_LIST:
    return (val_type(b) == VAL_LIST) &&
           list_eq(val_ptr2list(a), val_ptr2list(b));
  case VAL_SYMBOL:
    return false; // because of the early val_biteq
  default:
    assert(0);
  }
compare_strings:
  switch (val_type(b)) {
  case VAL_STRING: {
    String *bs = val_ptr2string(b);
    b_hash = bs->hash;
    b_len = bs->len;
    b_c = bs->c;
  }
  case VAL_SLICE: {
    Slice *bs = val_ptr2slice(b);
    b_hash = bs->hash;
    b_len = bs->len;
    b_c = bs->c;
  }
  default:
    return false;
  }
  if (a_len != b_len) {
    return false;
  }
  if ((a_hash != 0) && (b_hash != 0) && (a_hash != b_hash)) {
    return false;
  }
  return memcmp(a_c, b_c, b_len) == 0;
}

extern inline double val_data2double(Val);
extern inline Val val_data4double(double);
extern inline bool val_biteq(Val, Val);
extern inline bool val_is_ptr(Val);
extern inline bool val_is_data(Val);
extern inline bool val_is_ptr_own(Val);
extern inline bool val_is_ptr_ref(Val);
extern inline Val val_ptr2ref(Val);
extern inline void *val_ptr2ptr(Val);
extern inline ValType val_type(Val);
extern inline Val val_ptr4string(String *);
extern inline String *val_ptr2string(Val);
extern inline Val val_ptr4table(Table *);
extern inline Table *val_ptr2table(Val);
extern inline Val val_ptr4list(List *);
extern inline List *val_ptr2list(Val);
extern inline Val val_ptr4int64(int64_t *);
extern inline int64_t *val_ptr2int64(Val);
extern inline bool val_is_err(Val);
extern inline Val val_ptr4err(Val *);
extern inline Val *val_ptr2err(Val);
extern inline Val val_ptr4slice(Slice *);
extern inline Slice *val_ptr2slice(Val);
extern inline uint16_t val_data2pair_ua(Val);
extern inline uint32_t val_data2pair_ub(Val);
extern inline int16_t val_data2pair_a(Val);
extern inline int32_t val_data2pair_b(Val);
extern inline Val val_data4upair(uint16_t, uint32_t);
extern inline Val val_data4pair(int16_t, int32_t);
extern inline Symbol val_data2symbol(Val);
