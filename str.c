#include "str.h"

#include "mem.h" // ALLOCATE_FLEX, FREE_FLEX

#include <stdbool.h> // bool, false, true
#include <stdint.h>  // uint32_t, UINT32_MAX
#include <stdio.h>   // printf
#include <string.h>  // memcmp, memcpy, size_t

uint32_t str_hash(const char *c, size_t len) {
  uint32_t res = 2166136261u;
  for (uint32_t i = 0; i < len; i++) {
    res ^= c[i];
    res *= 16777619u;
  }
  return res;
}

static Str *str_new_sized(uint32_t len) {
  Str *res = ALLOCATE_FLEX(Str, char, (size_t)len);
  if (res == 0)
    return 0;
  *res = (Str){
      .len = len,
      .hash = 0,
  };
  return res;
}

Str *str_new(char const *c, uint32_t len) {
  Str *res = str_new_sized(len);
  if (res == 0)
    return 0;
  memcpy(res->c, c, len);
  res->hash = str_hash(c, len);
  return res;
}

void str_free(Str *str) { FREE_FLEX(str, Str, char, (size_t)str->len); }

Slice str_slice(Str const *str) {
  return (Slice){
      .len = str->len,
      .hash = str->hash, // avoid recomputing the hash
      .c = str->c,
  };
}

Slice *slice_init(Slice *slice, char const *c, uint32_t len) {
  if (slice == 0)
    return 0;
  *slice = (Slice){
      .len = len,
      .hash = str_hash(c, len),
      .c = c,
  };
  return slice;
}

void slice_print(Slice const *slice) { printf("%.*s", slice->len, slice->c); }

bool slice_equals(Slice const *a, Slice const *b) {
  if (a->len != b->len)
    return false;
  if (a->hash != b->hash)
    return false;
  if (a->c == b->c) // NB: compare ptrs only if len is eq
    return true;
  return memcmp(a->c, b->c, a->len) == 0;
}

Str *slice_concat(Slice const *a, Slice const *b) {
  uint64_t len = a->len + b->len;
  if (len > UINT32_MAX)
    return 0;
  Str *res = str_new_sized(len);
  if (res == 0)
    return 0;
  memcpy(res->c, a->c, a->len);
  memcpy(res->c + a->len, b->c, b->len);
  res->hash = str_hash(res->c, res->len);
  return res;
}
