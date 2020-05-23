#include "str.h"

#include "mem.h"      // mem_*
#include "safemath.h" // size_t_add_over

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

size_t str_hash(const char *c, size_t len) {
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

String *string_append(String *s, const char *c, size_t len) {
  size_t new_size;
  if (size_t_add_over(s->len, len, &new_size)) {
    mem_error("string size too large");
    return 0;
  }
  s = mem_resize_flex(s, sizeof(*s), sizeof(s->c[0]), s->len, new_size);
  memcpy(s->c + s->len, c, len);
  s->len = new_size;
  return s;
}

String *str_concat(const char *l, size_t l_len, const char *r, size_t r_len) {
  size_t new_size;
  if (size_t_add_over(l_len, r_len, &new_size)) {
    mem_error("string size too large");
    return 0;
  }
  String *s = mem_allocate_flex(sizeof(*s), sizeof(s->c[0]), new_size);
  s->len = new_size;
  s->hash = 0;
  memcpy(s->c, l, l_len);
  memcpy(s->c + l_len, r, r_len);
  return s;
}

extern inline String *string_new(const char *, size_t);
extern void string_free(String *);
extern inline Slice slice(const char *, const char *);
extern inline void slice_free(Slice *);

extern inline void string_printf(FILE *, const String *);
extern inline void slice_printf(FILE *, const Slice *);
extern inline void string_reprf(FILE *, const String *);
extern inline void slice_reprf(FILE *, const Slice *);

extern inline void string_print(const String *);
extern inline void slice_print(const Slice *);
extern inline void string_repr(const String *);
extern inline void slice_repr(const Slice *);

extern inline size_t string_len(const String *);
extern inline size_t slice_len(const Slice *);

extern inline size_t string_hash(String *);
extern inline size_t slice_hash(Slice *);

extern inline bool string_eq_string(const String *, const String *);
extern inline bool string_eq_slice(const String *, const Slice *);
extern inline bool slice_eq_slice(const Slice *, const Slice *);
extern inline bool slice_eq_string(const Slice *, const String *);

extern inline String *string_concat_string(const String *l, const String *);
extern inline String *string_concat_slice(const String *, const Slice *);
extern inline String *slice_concat_string(const Slice *, const String *);
extern inline String *slice_concat_slice(const Slice *, const Slice *);

extern inline int string_cmp_string(const String *, const String *);
extern inline int string_cmp_slice(const String *, const Slice *);
extern inline int slice_cmp_slice(const Slice *, const Slice *);
extern inline int slice_cmp_string(const Slice *, const String *);
