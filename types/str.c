#include "str.h"

#include "mem.h" // mem_free_flex, mem_free_array, mem_free

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t, UINT64_C
#include <stdio.h>   // fprintf, putc
#include <string.h>  // memcmp

String *string_new(const char *c, size_t len) {
  String *s = mem_allocate_flex(sizeof(String), sizeof(char), len);
  if (s) {
    s->len = len;
    s->hash = 0;
    memcpy(s->c, c, len);
  }
  return s;
}

void string_free(String *s) {
  mem_free_flex(s, sizeof(String), sizeof(char), s->len);
}

void slice_free(Slice *s) { mem_free(s, sizeof(Slice)); }

static void print(FILE *f, const char *c, size_t len) {
  // TODO: fix cast to int
  fprintf(f, "%.*s", (int)len, c);
}

void string_printf(FILE *f, const String *s) { print(f, s->c, s->len); }
void string_reprf(FILE *f, const String *s) {
  putc('"', f);
  print(f, s->c, s->len);
  putc('"', f);
}
void slice_printf(FILE *f, const Slice *s) { print(f, s->c, s->len); }
void slice_reprf(FILE *f, const Slice *s) {
  putc('"', f);
  print(f, s->c, s->len);
  putc('"', f);
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

#define STR_EQ_STR                                                             \
  if (a->len != b->len) {                                                      \
    return false;                                                              \
  }                                                                            \
  if (a->hash != 0 && b->hash != 0 && a->hash != b->hash) {                    \
    return false;                                                              \
  }                                                                            \
  return memcmp(a->c, b->c, a->len) == 0;

bool string_eq_string(const String *a, const String *b) { STR_EQ_STR; }
bool string_eq_slice(const String *a, const Slice *b) { STR_EQ_STR; }
bool slice_eq_slice(const Slice *a, const Slice *b) { STR_EQ_STR; }
#undef STR_EQ_STR

extern inline size_t string_len(const String *);
extern inline size_t slice_len(const Slice *);
extern inline Slice slice(const char *start, const char *end);

extern inline void string_print(const String *);
extern inline void string_repr(const String *);
extern inline void slice_print(const Slice *);
extern inline void slice_repr(const Slice *);
