#include "types/tag.h"

#include "mem.h" // mem_allocation_summary

#include <stdlib.h> // EXIT_SUCCESS

struct String {
  char _;
};

struct Table {
  char _;
};

struct List {
  char _;
};

struct Slice {
  char _;
};

static String *string_freed = 0;
void string_free(String *s) { string_freed = s; }
static Table *table_freed = 0;
void table_free(Table *t) { table_freed = t; }
static List *list_freed = 0;
void list_free(List *l) { list_freed = l; }
static Slice *slice_freed = 0;
void slice_free(Slice *s) { slice_freed = s; }

static const String *string_printed;
void string_print(const String *s) { string_printed = s; }
static const String *string_repred = 0;
void string_repr(const String *s) { string_repred = s; }
static const Table *table_printed = 0;
void table_print(const Table *t) { table_printed = t; }
static const List *list_printed = 0;
void list_print(const List *l) { list_printed = l; }
static const Slice *slice_printed = 0;
void slice_print(const Slice *s) { slice_printed = s; }
static const Slice *slice_repred = 0;
void slice_repr(const Slice *s) { slice_repred = s; }

static size_t string_len_ret = 0;
size_t string_len(const String *s) { return string_len_ret; }
static size_t table_len_ret = 0;
size_t table_len(const Table *t) { return table_len_ret; }
static size_t list_len_ret = 0;
size_t list_len(const List *l) { return list_len_ret; }
static size_t slice_len_ret = 0;
size_t slice_len(const Slice *s) { return slice_len_ret; }

static size_t string_hash_ret = 0;
size_t string_hash(String *s) { return string_hash_ret; }
static size_t slice_hash_ret = 0;
size_t slice_hash(Slice *s) { return slice_hash_ret; }

static bool string_eq_string_ret = 0;
bool string_eq_string(const String *a, const String *b) {
  return string_eq_string_ret;
}
static bool string_eq_slice_ret = 0;
bool string_eq_slice(const String *a, const Slice *b) {
  return string_eq_slice_ret;
}
static bool slice_eq_slice_ret = 0;
bool slice_eq_slice(const Slice *a, const Slice *b) {
  return slice_eq_slice_ret;
}
static bool table_eq_ret = 0;
bool table_eq(const Table *a, const Table *b) { return table_eq_ret; }
static bool list_eq_ret = 0;
bool list_eq(const List *a, const List *b) { return list_eq_ret; }

static void *mem_reallocate_p = 0;
static size_t mem_reallocate_old_size = 0;
static size_t mem_reallocate_new_size = 0;
static void *mem_reallocate_ret = 0;
void *mem_reallocate(void *p, size_t old_size, size_t new_size) {
  mem_reallocate_p = p;
  mem_reallocate_old_size = old_size;
  mem_reallocate_new_size = new_size;
  return mem_reallocate_ret;
}

int main(void) {

  Tag t;

  double double_ = 1234567890.1234567890;
  t = double_to_tag(double_);
  assert(tag_is_double(t) && "double check");
  assert((tag_to_double(t) == double_) && "double conversion");
  assert(!tag_is_ptr(t) && "double pointer check");
  assert(tag_is_data(t) && "double data check");
  assert(!tag_is_own(t) && "double pointer ownership check");
  assert(!tag_is_ref(t) && "double pointer reference check");

  assert(tag_biteq(t, t) && "bit-wise equality");

  struct String string;
  t = string_to_tag(&string);
  assert(tag_is_string(t) && "string check");
  assert((tag_to_string(t) == &string) && "string conversion");
  assert(tag_is_ptr(t) && "string pointer check");
  assert(!tag_is_data(t) && "string data check");
  assert(tag_is_own(t) && "string pointer ownership check");

  assert(!tag_is_ref(t) && "string pointer reference check");
  Tag ref = tag_to_ref(t);
  assert(tag_is_ref(ref) && "reference check");
  assert(tag_is_ptr(ref) && "reference pointer check");
  assert((tag_to_string(ref) == &string) && "reference string conversion");

  struct Table table;
  t = table_to_tag(&table);
  assert(tag_is_table(t) && "table check");
  assert((tag_to_table(t) == &table) && "table conversion");
  assert(tag_is_ptr(t) && "table pointer check");
  assert(!tag_is_data(t) && "table data check");
  assert(tag_is_own(t) && "table pointer ownership check");

  struct Slice slice;
  t = slice_to_tag(&slice);
  assert(tag_is_slice(t) && "slice check");
  assert((tag_to_slice(t) == &slice) && "slice conversion");
  assert(tag_is_ptr(t) && "slice pointer check");
  assert(!tag_is_data(t) && "slice data check");
  assert(tag_is_own(t) && "slice pointer ownership check");

  t = pair_to_tag(-7, -8);
  assert(tag_is_pair(t) && "pair check");
  assert((tag_to_pair_a(t) == -7) && "pair a conversion");
  assert((tag_to_pair_b(t) == -8) && "pair b conversion");
  assert(!tag_is_ptr(t) && "pair pointer check");
  assert(tag_is_data(t) && "pair data check");
  t = upair_to_tag(7, 8);
  assert(tag_is_pair(t) && "upair check");
  assert((tag_to_pair_ua(t) == 7) && "pair ua conversion");
  assert((tag_to_pair_ub(t) == 8) && "pair ub conversion");
  assert(!tag_is_ptr(t) && "upair pointer check");
  assert(tag_is_data(t) && "upair data check");

  assert(tag_is_symbol(TAG_FALSE) && "false symbol check");
  assert(!tag_is_ptr(TAG_FALSE) && "false pointer check");
  assert(tag_is_data(TAG_FALSE) && "false data check");
  assert(tag_to_symbol(TAG_FALSE) == SYM_FALSE && "false symbol equality");

  assert(tag_is_symbol(TAG_TRUE) && "true symbol check");
  assert(tag_to_symbol(TAG_TRUE) == SYM_TRUE && "true symbol equality");
  assert(tag_is_symbol(TAG_NIL) && "nil symbol check");
  assert(tag_to_symbol(TAG_NIL) == SYM_NIL && "nil symbol equality");
  assert(tag_is_symbol(TAG_OK) && "ok symbol check");
  assert(tag_to_symbol(TAG_OK) == SYM_OK && "ok symbol equality");

  tag_free(double_to_tag(10.0));
  assert(mem_reallocate_p == 0 && "double free");
  tag_free(TAG_TRUE);
  assert(mem_reallocate_p == 0 && "symbol free");
  tag_free(upair_to_tag(0, 0));
  assert(mem_reallocate_p == 0 && "pair free");

#ifdef CLOX_DEBUG
  mem_allocation_summary();
#endif
  return EXIT_SUCCESS;
}
