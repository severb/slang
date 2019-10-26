#include "str.h"

#include "mem.h" // ALLOCATE_FLEX, FREE_FLEX

#include <stdbool.h> // bool, false, true
#include <stdint.h>  // uint64_t, uint32_t, UINT32_MAX
#include <stdio.h>   // printf
#include <string.h>  // memcpy, size_t

uint32_t chars_hash(const char *c, uint32_t len) {
  uint32_t res = 2166136261u;
  for (uint32_t i = 0; i < len; i++) {
    res ^= c[i];
    res *= 16777619u;
  }
  if (!res)
    res = 7777777;
  return res;
}

static String *string_new_sized(uint32_t len) {
  String *res = ALLOCATE_FLEX(String, char, (size_t)len);
  if (res == 0)
    return 0;
  *res = (String){
      .len = len,
      .hash = 0,
  };
  return res;
}

String *chars_concat(char const *a, uint32_t len_a, char const *b,
                     uint32_t len_b) {
  uint64_t len = len_a + len_b;
  if (len > UINT32_MAX)
    return 0;
  String *res = string_new_sized(len);
  if (res == 0)
    return 0;
  memcpy(res->c, a, len_a);
  memcpy(res->c + len_a, b, len_b);
  return res;
}

void chars_print(char const *c, uint32_t len) { printf("%.*s", len, c); }

String *string_new(char const *c, uint32_t len) {
  String *res = string_new_sized(len);
  if (res == 0)
    return 0;
  memcpy(res->c, c, len);
  return res;
}

void string_free(String *s) { FREE_FLEX(s, String, char, (size_t)s->len); }

bool slice_equals(Slice *a, Slice *b) {
  if (a->len != b->len)
    return false;
  if (S_HASH(a) != S_HASH(b))
    return false;
  if (a->c == b->c) // NB: compare ptrs only if len is eq
    return true;
  return memcmp(a->c, b->c, a->len) == 0;
}
