#include "tag.h" // Tag, tag_*, *_tag

#include <stdlib.h>

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

  t = i49_to_tag(7);
  assert(tag_is_i49(t) && "positive i49 check");
  assert((tag_to_i49(t) == 7) && "positive i49 conversion");
  assert(!tag_is_ptr(t) && "positive i49 pointer check");
  assert(tag_is_data(t) && "positive i49 data check");

  t = i49_to_tag(-7);
  assert(tag_is_i49(t) && "negative i49 check");
  assert((tag_to_i49(t) == -7) && "negative i49 conversion");
  assert(!tag_is_ptr(t) && "negative i49 pointer check");
  assert(tag_is_data(t) && "negative i49 data check");

  t = i49_to_tag(10);
  assert((tag_to_i49(i49_negate(t)) == -10) && "i49 negate");

  t = int_to_tag(I49_MAX);
  assert(tag_is_i49(t) && "I49_MAX check");
  t = int_to_tag(I49_MIN);
  assert(tag_is_i49(t) && "I49_MIN check");

  t = int_to_tag(I49_MAX + 1);
  assert(tag_is_i64(t) && "I49_MAX check");
  t = int_to_tag(I49_MIN - 1);
  assert(tag_is_i64(t) && "I49_MIN check");

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

  return EXIT_SUCCESS;
}
