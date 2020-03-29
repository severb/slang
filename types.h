#ifndef clox_types_h
#define clox_types_h

#include "val.h" // Val

#include <stdio.h> // printf, size_t

size_t chars_hash(const char *, size_t len);

typedef struct String {
  size_t len;
  size_t hash;
  char c[]; // flexible array member
} String;

inline void string_print(const String *s) {
  // TODO: what happens if the conversion fails?
  printf("%.*s", (int)s->len, s->c);
}
String *string_new_empty(size_t len);
String *string_new(const char *, size_t len);
inline size_t string_hash(String *s) {
  return s->hash == 0 ? (s->hash = chars_hash(s->c, s->len)) : s->hash;
}
#define STRING(s) (string_new(#s, sizeof(#s) - 1))

typedef struct Slice {
  size_t len;
  size_t hash;
  const char *c;
} Slice;

inline void slice_print(const Slice *s) {
  // TODO: what happens if the conversion fails?
  printf("%.*s", (int)s->len, s->c);
}
inline Slice slice(const char *c, size_t len) {
  return (Slice){.c = c, .len = len, .hash = 0};
}
inline size_t slice_hash(Slice *s) {
  return s->hash == 0 ? (s->hash = chars_hash(s->c, s->len)) : s->hash;
}
#define SLICE(s) ((Slice){.c = #s, .len = sizeof(#s) - 1})

typedef struct List {
  size_t cap;
  size_t len;
  Val *items;
} List;

bool list_eq(const List *, const List *);
void list_free(List *);
void list_print(const List *);
inline Val list_get(const List *l, size_t idx, Val def) {
  return idx < l->len ? l->items[idx] : def;
}
inline void list_set(List *l, size_t idx, Val val) {
  assert(idx < l->len);
  val_free(l->items[idx]);
  l->items[idx] = val;
}
inline Val list_pop(List *l, Val def) {
  return l->len > 0 ? l->items[l->len--] : def;
}
void list_append(List *, Val);

typedef struct Entry {
  Val key;
  Val val;
} Entry;

typedef struct Table {
  size_t cap;
  size_t len;
  Entry *items;
  size_t real_len;
} Table;

bool table_eq(const Table *, const Table *);
void table_free(Table *);
void table_print(const Table *);
bool table_set(Table *, Val key, Val val);
Val table_get(const Table *, Val key, Val def);
bool table_del(Table *, Val);

// CLOX_DEBUG stuff
void table_summary(const Table *);
void collision_summary(void);
void collision_reset(void);

#endif
