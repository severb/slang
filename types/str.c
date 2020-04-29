#include "types/str.h"
#include "mem.h"

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t, UINT64_C
#include <stdio.h>   // printf
#include <string.h>  // memcmp

void string_free(String *s) {
  mem_free_flex(s, sizeof(String), sizeof(char), s->len);
}

void slice_free(Slice *s) {
  mem_free_array(s, sizeof(char), sizeof(s->len));
  mem_free(s, sizeof(Slice));
}

static void print(const char *c, size_t len) {
  // TODO: fix cast to int
  printf("%.*s", (int)len, c);
}

void string_print(const String *s) { print(s->c, s->len); }
void string_repr(const String *s) {
  putchar('"');
  print(s->c, s->len);
  putchar('"');
}
void slice_print(const Slice *s) { print(s->c, s->len); }
void slice_repr(const Slice *s) {
  putchar('"');
  print(s->c, s->len);
  putchar('"');
}

static size_t hash(const char *c, size_t len) {
  uint64_t res = UINT64_C(2166136261);
  for (size_t i = 0; i < len; i++) {
    res ^= c[i];
    res *= UINT64_C(16777619);
  }
  if (res == 0) { // avoid zero-hashes because 0 indicates no hash is cached
    res = UINT64_C(0x1337);
  }
  return res;
}

size_t string_hash(String *s) {
  return s->hash ? s->hash : (s->hash = hash(s->c, s->len));
}
size_t slice_hash(Slice *s) {
  return s->hash ? s->hash : (s->hash = hash(s->c, s->len));
}

#define str_eq_str                                                             \
  if (a->len != b->len) {                                                      \
    return false;                                                              \
  }                                                                            \
  if (a->hash != 0 && b->hash != 0 && a->hash != b->hash) {                    \
    return false;                                                              \
  }                                                                            \
  return memcmp(a->c, b->c, a->len)

bool string_eq_string(const String *a, const String *b) { str_eq_str; }
bool string_eq_slice(const String *a, const Slice *b) { str_eq_str; }
bool slice_eq_slice(const Slice *a, const Slice *b) { str_eq_str; }

extern inline size_t string_len(const String *);
extern inline size_t slice_len(const Slice *);
