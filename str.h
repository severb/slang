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

String *string_new(char const *, uint32_t len);
#define STRING(s) (string_new(#s, sizeof(#s) - 1))

void string_free(String *);

typedef struct Slice {
  uint32_t len;
  uint32_t hash;
  char const *c;
} Slice;

// chars_hash() returns a non-zero hash value.
uint32_t chars_hash(char const *, uint32_t len);

#define unsafe_hash(s)                                                         \
  ((s)->hash > 0 ? (s)->hash                                                   \
                 : ((s)->hash = (chars_hash)((s)->c, (s)->len), (s)->hash))

#define slice_(ch, ln) ((Slice){.c = (ch), .len = (ln), .hash = 0})
static inline Slice slice(char const *c, uint32_t len) {
  return slice_(c, len);
}
#define SLICE(s) ((Slice){.c = #s, .len = sizeof(#s) - 1})

String *chars_concat(char const *a, uint32_t len_a, char const *b,
                     uint32_t len_b);
#define unsafe_concat(a, b) ((chars_concat)((a).c, (a).len, (b).c, (b).len))

void chars_print(char const *, uint32_t);
#define unsafe_print(a) ((chars_print)((a).c, (a).len))

// string_slice() convert a String into a Slice.
static inline Slice string_slice(String *s) {
  return (Slice){
      .len = s->len,
      .hash = unsafe_hash(s), // avoid recomputing the hash
      .c = s->c,
  };
}

#define unsafe_equals(a, b)                                                    \
  (((a)->len == (b)->len) &&                                                   \
   (unsafe_hash(a) == unsafe_hash(b) && (a)->c == (b)->c ||                    \
    memcmp((a)->c, (b)->c, (a)->len) == 0))

#endif
