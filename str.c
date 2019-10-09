#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "mem.h"
#include "str.h"

uint32_t str_hash(const char *c, size_t len) {
  uint32_t res = 2166136261u;
  for (int i = 0; i < len; i++) {
    res ^= c[i];
    res *= 16777619u;
  }
  return res;
}

static Str *str_new_sized(uint32_t len) {
  Str *res = ALLOCATE_FLEX(Str, char, len);
  if (res == 0)
    return 0;
  *res = (Str){
      .len = len,
      .hash = 0,
  };
  return res;
}

Str *str_new(const char *c, uint32_t len) {
  Str *res = str_new_sized(len);
  if (res == 0)
    return 0;
  memcpy(res->c, c, len);
  res->hash = str_hash(c, len);
  return res;
}

void str_free(Str *str) { FREE_FLEX(str, Str, char, str->len); }

Str *str_copy(Str *str) {
  // Avoid recomputing the hash.
  Str *res = str_new_sized(str->len);
  if (res == 0)
    return 0;
  memcpy(res->c, str->c, str->len);
  res->hash = str->hash;
  return res;
}

Slice str_slice(Str *str) {
  assert(str->len <= MAX_SLICE_LEN);
  return (Slice){
      .len = str->len,
      // Avoid recomputing the hash.
      .hash = str->hash,
      .c = str->c,
  };
}

Slice *slice_init(Slice *slice, const char *c, uint32_t len) {
  assert(len <= MAX_SLICE_LEN);
  if (slice == 0)
    return 0;
  *slice = (Slice){
      .len = len,
      .hash = str_hash(c, len),
      .c = c,
  };
  return slice;
}

Str *slice_str(Slice slice) {
  // Avoid recomputing the hash.
  Str *res = str_new_sized(slice.len);
  if (res == 0)
    return 0;
  memcpy(res->c, slice.c, slice.len);
  res->hash = slice.hash;
  return res;
}

void slice_print(Slice slice) { printf("%.*s", slice.len, slice.c); }

bool slice_equals(Slice a, Slice b) {
  if (a.len != b.len)
    return false;
  if (a.hash != b.hash)
    return false;
  if (a.c == b.c) // NB: compare ptrs only if len is eq
    return true;
  return memcmp(a.c, b.c, a.len) == 0;
}

Str *slice_concat(Slice a, Slice b) {
  uint32_t len = a.len + b.len;
  Str *res = str_new_sized(len);
  if (res == 0)
    return 0;
  memcpy(res->c, a.c, a.len);
  memcpy(res->c + a.len, b.c, b.len);
  res->hash = str_hash(res->c, res->len);
  return res;
}
