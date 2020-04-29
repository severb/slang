#ifndef slang_str_h
#define slang_str_h

#include <stdbool.h> // bool
#include <stddef.h>  // size_t

typedef struct String {
  size_t len;
  size_t hash;
  char c[];
} String;

typedef struct Slice {
  size_t len;
  size_t hash;
  char *c;
} Slice;

void string_free(String *);
void slice_free(Slice *);

void string_print(const String *);
void string_repr(const String *);
void slice_print(const Slice *);
void slice_repr(const Slice *);

inline size_t string_len(const String *s) { return s->len; }
inline size_t slice_len(const Slice *s) { return s->len; }

size_t string_hash(String *);
size_t slice_hash(Slice *);

bool string_eq_string(const String *, const String *);
bool string_eq_slice(const String *, const Slice *);
bool slice_eq_slice(const Slice *, const Slice *);

#endif
