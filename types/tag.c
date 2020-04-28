#include "types/tag.h"

#include "mem.h" // mem_free

#include <inttypes.h> // PRId*
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t, max_align_t
#include <stdint.h>   // uint*_t, int*_t, uintptr_t, UINT64_C
#include <stdio.h>    // printf

void string_free(String *);
void table_free(Table *);
void list_free(List *);
void slice_free(Slice *);

void string_print(const String *);
void string_repr(const String *);
void table_print(const Table *);
void list_print(const List *);
void slice_print(const Slice *);
void slice_repr(const Slice *);

size_t string_len(const String *);
size_t table_len(const Table *);
size_t list_len(const List *);
size_t slice_len(const Slice *);

size_t string_hash(String *);
size_t slice_hash(Slice *);

bool string_eq_string(const String *, const String *);
bool string_eq_slice(const String *, const Slice *);
bool slice_eq_slice(const Slice *, const Slice *);
bool table_eq(const Table *, const Table *);
bool list_eq(const List *, const List *);

void tag_free(Tag t) {
  if (!tag_is_own(t)) {
    return; // only free owned pointers
  }
  switch (tag_type(t)) {
  case TYPE_STRING:
    string_free(tag_to_string(t));
    break;
  case TYPE_TABLE:
    table_free(tag_to_table(t));
    break;
  case TYPE_LIST:
    list_free(tag_to_list(t));
    break;
  case TYPE_I64:
    mem_free(tag_to_i64(t), sizeof(uint64_t));
  case TYPE_ERROR: {
    Tag *error = tag_to_error(t);
    t = *error;
    mem_free(error, sizeof(Tag));
    tag_free(t);
    break;
  }
  case TYPE_SLICE:
    slice_free(tag_to_slice(t));
    break;
  default:
    assert(0 && "tag_free called on unknown tag type");
  }
}

static const char *symbols[] = {"<false>", "<true>", "<nil>", "<ok>"};
static void print(Tag t, bool is_repr) {
  switch (tag_type(t)) {
  case TYPE_STRING:
    if (is_repr) {
      string_repr(tag_to_string(t));
    } else {
      string_print(tag_to_string(t));
    }
    break;
  case TYPE_TABLE:
    table_print(tag_to_table(t));
    break;
  case TYPE_LIST:
    list_print(tag_to_list(t));
    break;
  case TYPE_I64:
    printf("%" PRId64, *tag_to_i64(t));
    break;
  case TYPE_ERROR:
    puts("error: ");
    print(*tag_to_error(t), true);
    break;
  case TYPE_SLICE:
    if (is_repr) {
      slice_repr(tag_to_slice(t));
    } else {
      slice_print(tag_to_slice(t));
    }
    break;
  case TYPE_PAIR:
    printf("(%" PRId16 ", %" PRId32 ")", tag_to_pair_a(t), tag_to_pair_b(t));
    break;
  case TYPE_DOUBLE:
    printf("%f", tag_to_double(t));
    break;
  case TYPE_SYMBOL: {
    Symbol s = tag_to_symbol(t);
    if (s >= SYM__COUNT) {
      printf("<symbol: %d>", s);
    } else {
      puts(symbols[s]);
    }
    break;
  }
  default:
    assert(0 && "tag print called on unknown tag type");
  }
}

void tag_print(Tag t) { print(t, false); }
void tag_repr(Tag t) { print(t, true); }

static size_t int_hash(uint64_t i) { return i * 13 + 37; }

size_t tag_hash(Tag t) {
  switch (tag_type(t)) {
  case TYPE_STRING:
    return 0xFEEDFEED ^ string_hash(tag_to_string(t));
  case TYPE_TABLE:
  case TYPE_LIST:
    return 0xDEADBEEF ^ (((uintptr_t)tag_to_ptr(t)) >> sizeof(max_align_t));
  case TYPE_I64:
    return int_hash(SAFE_CAST(int64_t, uint64_t, *tag_to_i64(t)));
  case TYPE_ERROR:
    return 0xC0FFEE ^ tag_hash(*tag_to_error(t));
  case TYPE_SLICE:
    return 0xFEEDFEED ^ slice_hash(tag_to_slice(t));
  case TYPE_DOUBLE: {
    double d = tag_to_double(t);
    if (d == (int64_t)d) {
      return int_hash(SAFE_CAST(int64_t, uint64_t, d));
    }
    return SAFE_CAST(double, size_t, d);
  }
  case TYPE_PAIR:
    return int_hash((((uint64_t)tag_to_pair_ua(t)) << 32) | tag_to_pair_ub(t));
  case TYPE_SYMBOL:
    return 0xCACA0 ^ (tag_to_symbol(t) * 31 + 37);
  }
}

bool tag_is_true(Tag t) {
  switch (tag_type(t)) {
  case TYPE_STRING:
    return string_len(tag_to_string(t));
  case TYPE_TABLE:
    return table_len(tag_to_table(t));
  case TYPE_LIST:
    return list_len(tag_to_list(t));
  case TYPE_I64:
    return *tag_to_i64(t);
  case TYPE_ERROR:
    return false;
  case TYPE_SLICE:
    return slice_len(tag_to_slice(t));
  case TYPE_DOUBLE:
    return tag_to_double(t);
  case TYPE_PAIR:
    return tag_to_pair_ua(t) || tag_to_pair_ub(t);
  case TYPE_SYMBOL:
    switch (tag_to_symbol(t)) {
    case SYM_TRUE:
    case SYM_OK:
      return true;
    default:
      return false;
    }
  default:
    assert(0 && "tag_is_true called on unknown tag type");
  }
}

bool tag_eq(Tag a, Tag b) {
  if (tag_biteq(a, b)) {
    return true;
  }
  if (tag_is_ptr(a) && tag_is_ptr(b) && tag_to_ptr(a) == tag_to_ptr(b)) {
    // compare pointers disregarding ownership
    return true;
  }
  switch (tag_type(a)) {
  case TYPE_STRING:
    if (tag_is_string(b)) {
      return string_eq_string(tag_to_string(a), tag_to_string(b));
    } else if (tag_is_slice(b)) {
      return string_eq_slice(tag_to_string(a), tag_to_slice(b));
    }
    return false;
  case TYPE_TABLE:
    return tag_is_table(b) && table_eq(tag_to_table(a), tag_to_table(b));
  case TYPE_LIST:
    return tag_is_list(b) && list_eq(tag_to_list(a), tag_to_list(b));
  case TYPE_I64:
    if (tag_is_pair(b)) {
      return tag_to_pair_ua(b) == 0 && *tag_to_i64(a) == tag_to_pair_b(b);
    } else if (tag_is_i64(b)) {
      return *tag_to_i64(a) == *tag_to_i64(b);
    } else if (tag_is_double(b)) {
      return *tag_to_i64(a) == tag_to_double(b);
    }
    return false;
  case TYPE_ERROR:
    return tag_is_error(b) && tag_eq(*tag_to_error(a), *tag_to_error(b));
  case TYPE_SLICE:
    if (tag_is_slice(b)) {
      return slice_eq_slice(tag_to_slice(a), tag_to_slice(b));
    } else if (tag_is_string(b)) {
      return string_eq_slice(tag_to_string(b), tag_to_slice(a));
    }
    return false;
  case TYPE_DOUBLE:
    if (tag_is_double(b)) {
      return tag_to_double(a) == tag_to_double(b);
    } else if (tag_is_pair(b)) {
      return tag_to_pair_ua(b) == 0 && tag_to_double(a) == tag_to_pair_b(b);
    } else if (tag_is_i64(b)) {
      return tag_to_double(a) == *tag_to_i64(b);
    }
    return false;
  case TYPE_PAIR:
    if (tag_is_pair(b)) {
      return false; // should be tag_biteq() called at the top
    } else if (tag_to_pair_ua(a) == 0) {
      if (tag_is_i64(b)) {
        return tag_to_pair_b(a) == *tag_to_i64(b);
      } else if (tag_is_double(b)) {
        return tag_to_pair_b(a) == tag_to_double(b);
      }
    }
  case TYPE_SYMBOL:
    return false; // should be tag_biteq() called at the top
  default:
    assert(0 && "tag_eq called on unknown tag type");
  }
}

Tag tag_add(Tag a, Tag b) { return a; }

extern inline bool tag_biteq(Tag, Tag);
extern inline bool tag_is_ptr(Tag);
extern inline bool tag_is_data(Tag);
extern inline bool tag_is_own(Tag);
extern inline bool tag_is_ref(Tag);
extern inline Tag tag_to_ref(Tag);
extern inline void *tag_to_ptr(Tag);

extern inline bool tag_is_string(Tag);
extern inline Tag string_to_tag(const String *);
extern inline String *tag_to_string(Tag);

extern inline bool tag_is_table(Tag);
extern inline Tag table_to_tag(const Table *);
extern inline Table *tag_to_table(Tag);

extern inline bool tag_is_list(Tag);
extern inline Tag list_to_tag(const List *);
extern inline List *tag_to_list(Tag);

extern inline bool tag_is_i64(Tag);
extern inline Tag i64_to_tag(const int64_t *);
extern inline int64_t *tag_to_i64(Tag);

extern inline bool tag_is_error(Tag);
extern inline Tag error_to_tag(const Tag *);
extern inline Tag *tag_to_error(Tag);

extern inline bool tag_is_slice(Tag);
extern inline Tag slice_to_tag(const Slice *);
extern inline Slice *tag_to_slice(Tag);

extern inline bool tag_is_pair(Tag);
extern inline Tag upair_to_tag(uint16_t, uint32_t);
extern inline Tag pair_to_tag(int16_t a, int32_t b);
extern inline uint16_t tag_to_pair_ua(Tag);
extern inline uint32_t tag_to_pair_ub(Tag);
extern inline int16_t tag_to_pair_a(Tag);
extern inline int32_t tag_to_pair_b(Tag);

extern inline bool tag_is_symbol(Tag);
extern inline Symbol tag_to_symbol(Tag);

extern inline bool tag_is_double(Tag);
extern inline Tag double_to_tag(double);
extern inline double tag_to_double(Tag);

extern inline TagType tag_type(Tag);
