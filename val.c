#include "val.h"

#include "list.h" // list_destroy
#include "mem.h"  // FREE
#include "str.h"  // string_free
//#include "table.h" // table_destroy

#include <inttypes.h> // PRId64
#include <stddef.h>   // max_align_t
#include <stdint.h>   // uint64_t, intptr_t
#include <stdio.h>    // printf

void val_destroy(Val v) {
  if (is_ptr_own(v)) {
    switch (val_u(v) & TYPE_MASK) {
    case STRING_PTR_TYPE:
      string_free(string_ptr(v));
      break;
    case TABLE_PTR_TYPE:
      // table_destroy(table_ptr(v));
      break;
    case LIST_PTR_TYPE:
      list_destroy(list_ptr(v));
      break;
    case INT_PTR_TYPE:
      FREE(int_ptr(v), uint64_t);
      break;
    case ERR_PTR_TYPE:
      val_destroy(*err_ptr(v));
      FREE(err_ptr(v), Val);
      break;
    case SLICE_PTR_TYPE:
      FREE(slice_ptr(v), Slice);
      break;
    default:
      assert(0);
    }
  }
}

void val_print(Val v) {
  switch (val_u(v) & TYPE_MASK) {
  case STRING_PTR_TYPE:
    S_PRINT(*string_ptr(v));
    break;
  case TABLE_PTR_TYPE:
    // printf("<table[%zu]>", table_ptr(v)->len);
    break;
  case LIST_PTR_TYPE:
    printf("<list[%zu]>", list_ptr(v)->len);
    break;
  case INT_PTR_TYPE:
    printf("%" PRId64, *int_ptr(v));
    break;
  case ERR_PTR_TYPE:
    printf("error: ");
    val_print(*err_ptr(v));
    break;
  case SLICE_PTR_TYPE:
    S_PRINT(*slice_ptr(v));
    break;
  case PAIR_DATA_TYPE:
    printf("(%" PRId16 ", %" PRId32 ")", pair_a(v), pair_b(v));
    break;
  case SYMB_DATA_TYPE:
    switch (val_u(v)) {
    case FALSE:
      printf("false");
      break;
    case TRUE:
      printf("true");
      break;
    case NIL:
      printf("nil");
      break;
    }
    break;
  default:
    assert(0);
  }
}

void val_print_repr(Val v) {
#define PTR_DETAILS printf("@%p (%s)", ptr(v), is_ptr_own(v) ? "o" : "r")
  switch (val_u(v) & TYPE_MASK) {
  case STRING_PTR_TYPE:
    printf("<STRING ");
    PTR_DETAILS;
    printf(" '");
    val_print(v);
    printf("'>");
    break;
  case TABLE_PTR_TYPE:
    // printf("<TABLE[%zu] ", table_ptr(v)->len);
    PTR_DETAILS;
    printf(">");
    break;
  case LIST_PTR_TYPE:
    printf("<LIST[%zu] ", list_ptr(v)->len);
    PTR_DETAILS;
    printf(">");
    break;
  case INT_PTR_TYPE:
    printf("<INT ");
    PTR_DETAILS;
    printf(" ");
    val_print(v);
    printf(">");
    break;
  case ERR_PTR_TYPE:
    printf("<ERR ");
    PTR_DETAILS;
    printf(" ");
    val_print_repr(*err_ptr(v));
    printf(">");
    break;
  case SLICE_PTR_TYPE:
    printf("<SLICE ");
    PTR_DETAILS;
    printf(" '");
    val_print(v);
    printf("'>");
    break;
  case PAIR_DATA_TYPE:
    printf("<PAIR ");
    printf(" ");
    val_print(v);
    printf(">");
    break;
  case SYMB_DATA_TYPE:
    printf("<SYMB ");
    val_print(v);
    printf(">");
    break;
  default:
    assert(0);
  }
#undef PTR_DETAILS
}

uint32_t val_hash(Val v) {
#define HASH_HALVES(i) ((i >> 32) ^ i)
  switch (val_u(v) & TYPE_MASK) {
  case STRING_PTR_TYPE:
    return S_HASH(string_ptr(v));
  case TABLE_PTR_TYPE:
  case LIST_PTR_TYPE:
    return (intptr_t)ptr(v) >> sizeof(max_align_t);
  case INT_PTR_TYPE: {
    uint64_t i = (uint64_t)*int_ptr(v);
    return HASH_HALVES(i);
  }
  case ERR_PTR_TYPE:
    return val_hash(*err_ptr(v)) + 17;
  case SLICE_PTR_TYPE:
    return S_HASH(slice_ptr(v));
  case PAIR_DATA_TYPE: // for data xor the halves
  case SYMB_DATA_TYPE:
    return HASH_HALVES(val_u(v));
  default:
    assert(0);
  }
#undef HASH_HALVES
}

bool val_truthy(Val v) { return !(is_nil(v) || is_false(v)); }

bool val_equals(Val a, Val b) {
  switch (val_u(a) & TYPE_MASK) {
  case SLICE_PTR_TYPE:
    return (val_u(a) == val_u(b)) ||
           (is_string_ptr(b) && S_EQUALS(slice_ptr(a), string_ptr(b))) ||
           (is_slice_ptr(b) && S_EQUALS(slice_ptr(a), slice_ptr(b)));
  case STRING_PTR_TYPE:
    return (val_u(a) == val_u(b)) ||
           (is_string_ptr(b) && S_EQUALS(string_ptr(a), string_ptr(b))) ||
           (is_slice_ptr(b) && S_EQUALS(string_ptr(a), slice_ptr(b)));
  case INT_PTR_TYPE:
    return (val_u(a) == val_u(b)) ||
           (is_int_ptr(b) && (*int_ptr(a) == *int_ptr(b))) ||
           (is_pair_data(b) && (pair_ua(b) == 0) && (*int_ptr(a) == pair_b(b)));
  case PAIR_DATA_TYPE:
    return (val_u(a) == val_u(b)) ||
           ((pair_ua(a) == 0) && is_int_ptr(b) && (pair_b(a) == *int_ptr(b)));
  case ERR_PTR_TYPE:
    return is_err_ptr(b) && val_equals(*err_ptr(a), *err_ptr(b));
  case TABLE_PTR_TYPE:
  case LIST_PTR_TYPE:
  case SYMB_DATA_TYPE:
    return val_u(a) == val_u(b);
  default:
    assert(0);
  }
}
