#ifndef slang_str_h
#define slang_str_h

#include <assert.h>  // assert
#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdio.h>   // FILE

typedef struct String {
  size_t len;
  size_t hash;
  char c[];
} String;

typedef struct Slice {
  size_t len;
  size_t hash;
  const char *c;
} Slice; // Slices don't own char *c

String *string_new(const char *, size_t);
void string_free(String *);

inline Slice slice(const char *start, const char *end) {
  assert(start <= end);
  return (Slice){.len = end - start, .c = start};
}

void slice_free(Slice *);

void string_printf(FILE *, const String *);
void string_reprf(FILE *, const String *);
void slice_printf(FILE *, const Slice *);
void slice_reprf(FILE *, const Slice *);

inline void string_print(const String *s) { string_printf(stdout, s); }
inline void string_repr(const String *s) { string_reprf(stdout, s); }
inline void slice_print(const Slice *s) { slice_printf(stdout, s); }
inline void slice_repr(const Slice *s) { slice_reprf(stdout, s); }

inline size_t string_len(const String *s) { return s->len; }
inline size_t slice_len(const Slice *s) { return s->len; }

size_t string_hash(String *);
size_t slice_hash(Slice *);

bool string_eq_string(const String *, const String *);
bool string_eq_slice(const String *, const Slice *);
bool slice_eq_slice(const Slice *, const Slice *);

String *string_append(String *, const char *, size_t);

String *str_concat(const char *, size_t, const char *, size_t);
inline String *string_concat_string(const String *l, const String *r) {
  return str_concat(l->c, l->len, r->c, r->len);
}
inline String *string_concat_slice(const String *l, const Slice *r) {
  return str_concat(l->c, l->len, r->c, r->len);
}
inline String *slice_concat_string(const Slice *l, const String *r) {
  return str_concat(l->c, l->len, r->c, r->len);
}
inline String *slice_concat_slice(const Slice *l, const Slice *r) {
  return str_concat(l->c, l->len, r->c, r->len);
}

#endif
