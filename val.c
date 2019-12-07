#include "val.h"

#include "list.h"  // list_destroy
#include "mem.h"   // FREE
#include "str.h"   // string_free
#include "table.h" // table_destroy

#include <inttypes.h> // PRId64, PRId32, PRId16
#include <stddef.h>   // max_align_t
#include <stdint.h>   // int64_t, uintptr_t
#include <stdio.h>    // printf

void val_destroy(Val v) {
  if (!is_ptr_own(v))
    return;
  switch (val_u(v) & TYPE_MASK) {
  case STRING_PTR_TYPE:
    string_free(string_ptr(v));
    break;
  case TABLE_PTR_TYPE:
    table_destroy(table_ptr(v));
    break;
  case LIST_PTR_TYPE:
    list_destroy(list_ptr(v));
    break;
  case INT_PTR_TYPE:
    FREE(int_ptr(v), int64_t);
    break;
  case ERR_PTR_TYPE:
    val_destroy(*err_ptr(v));
    FREE(err_ptr(v), Val); // TODO: is this right?
    break;
  case SLICE_PTR_TYPE:
    FREE(slice_ptr(v), Slice);
    break;
  default:
    assert(0);
  }
}

void val_print(Val v) {
  switch (val_u(v) & TYPE_MASK) {
  case STRING_PTR_TYPE: {
    String *s = string_ptr(v);
    unsafe_print(*s);
    break;
  }
  case TABLE_PTR_TYPE:
    printf("<table[%zu]>", table_ptr(v)->len);
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
  case SLICE_PTR_TYPE: {
    Slice s = *slice_ptr(v);
    unsafe_print(s);
    break;
  }
  case PAIR_DATA_TYPE:
    printf("(%" PRId16 ", %" PRId32 ")", pair_a(v), pair_b(v));
    break;
  case SYMB_DATA_TYPE:
    switch (val_u(v)) {
    case FALSEu:
      printf("false");
      break;
    case TRUEu:
      printf("true");
      break;
    case NILu:
      printf("nil");
      break;
    case OKu:
      printf("ok");
      break;
    default:
      if (is_usr_symbol(v))
        printf("<user symbol: %" PRIx64 ">", val_usr(v));
      else
        printf("<reserved symbol: %" PRIx16 ">", (uint16_t)val_u(v));
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
    printf("<TABLE>");
    printf("<TABLE[%zu] ", table_ptr(v)->len);
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
#define HASH_HALVES(i) ((((i) >> 32) ^ (i)) * 3)
  switch (val_u(v) & TYPE_MASK) {
  case STRING_PTR_TYPE: {
    String *s = string_ptr(v);
    return unsafe_hash(s);
  }
  case TABLE_PTR_TYPE:
  case LIST_PTR_TYPE: {
    return ((uintptr_t)ptr(v) >> sizeof(max_align_t)) * 3 + 5;
  }
  case INT_PTR_TYPE: {
    uint64_t i = (uint64_t)*int_ptr(v);
    return HASH_HALVES(i);
  }
  case ERR_PTR_TYPE:
    return val_hash(*err_ptr(v)) * 5 + 17;
  case SLICE_PTR_TYPE: {
    Slice *s = slice_ptr(v);
    return unsafe_hash(s);
  }
  case PAIR_DATA_TYPE: // for data, xor the halves
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
  case SLICE_PTR_TYPE: {
    if (val_biteq(a, b))
      return true;
    if (is_slice_ptr(b)) {
      Slice *a_ptr = slice_ptr(a);
      Slice *b_ptr = slice_ptr(b);
      return unsafe_equals(a_ptr, b_ptr);
    }
    if (is_string_ptr(b)) {
      Slice *a_ptr = slice_ptr(a);
      String *b_ptr = string_ptr(b);
      return unsafe_equals(a_ptr, b_ptr);
    }
    return false;
  }
  case STRING_PTR_TYPE:
    if (val_biteq(a, b))
      return true;
    if (is_slice_ptr(b)) {
      String *a_ptr = string_ptr(a);
      Slice *b_ptr = slice_ptr(b);
      return unsafe_equals(a_ptr, b_ptr);
    }
    if (is_string_ptr(b)) {
      String *a_ptr = string_ptr(a);
      String *b_ptr = string_ptr(b);
      return unsafe_equals(a_ptr, b_ptr);
    }
    return false;
  case INT_PTR_TYPE:
    return val_biteq(a, b) || (is_int_ptr(b) && (*int_ptr(a) == *int_ptr(b))) ||
           (is_pair_data(b) && (pair_ua(b) == 0) && (*int_ptr(a) == pair_b(b)));
  case PAIR_DATA_TYPE:
    return val_biteq(a, b) ||
           ((pair_ua(a) == 0) && is_int_ptr(b) && (pair_b(a) == *int_ptr(b)));
  case ERR_PTR_TYPE:
    return is_err_ptr(b) && val_equals(*err_ptr(a), *err_ptr(b));
  case TABLE_PTR_TYPE:
  case LIST_PTR_TYPE:
  case SYMB_DATA_TYPE:
    return val_biteq(a, b);
  default:
    assert(0);
  }
}
