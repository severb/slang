#ifndef clox_str_h
#define clox_str_h

#include <stdbool.h> // bool
#include <stdint.h>  // uint32_t
#include <string.h>  // memcmp

typedef struct String {
  uint32_t len;
  uint32_t hash;
  char c[];
} String;

String *string_new(const char *, uint32_t len);
void string_free(String *);

typedef struct Slice {
  uint32_t len;
  uint32_t hash;
  const char *c;
} Slice;

// chars_hash() return a hash value > 0.
uint32_t chars_hash(char const *, uint32_t len);
// S_HASH() compute and cache a String or Slice hash.
#define S_HASH(s)                                                              \
  ((s)->hash > 0 ? (s)->hash                                                   \
                 : ((s)->hash = chars_hash((s)->c, (s)->len), (s)->hash))

// slice() initialize a Slice.
static inline Slice slice(char const *c, uint32_t len) {
  return (Slice){
      .c = c,
      .len = len,
      .hash = 0,
  };
}
// TXT_SLICE() create slices out of C string literals.
#define TXT_SLICE(s) slice((s), sizeof(s) - 1)

String *chars_concat(char const *a, uint32_t len_a, char const *b,
                     uint32_t len_b);
// CONCAT() any String/Slice combo.
#define S_CONCAT(a, b) chars_concat((a).c, (a).len, (b).c, (b).len)

void chars_print(char const *, uint32_t);
// PRINT() etiher a String or a Slice.
#define S_PRINT(a) chars_print((a).c, (a).len)

// string_slice() convert a String into a Slice.
static inline Slice string_slice(String *s) {
  return (Slice){
      .len = s->len,
      .hash = S_HASH(s), // avoid recomputing the hash
      .c = s->c,
  };
}

// EQUALS() compares any String/Slice combo.
#define S_EQUALS(a, b)                                                         \
  (((a)->len == (b)->len) && (S_HASH(a) == S_HASH(b) && (a)->c == (b)->c ||    \
                              memcmp((a)->c, (b)->c, (a)->len) == 0))

#endif
