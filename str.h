#ifndef clox_str_h
#define clox_str_h

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

typedef struct {
  uint32_t len;
  uint32_t hash;
  char c[];
} Str;

typedef struct {
  uint32_t len;
  uint32_t hash;
  const char *c;
} Slice;

uint32_t str_hash(char const *, size_t len);

Str *str_new(const char *, uint32_t len);
void str_free(Str *);
Slice str_slice(Str const *);

Slice *slice_init(Slice *, char const *, uint32_t len);
void slice_print(Slice const *);
bool slice_equals(Slice const *, Slice const *);
Str *slice_concat(Slice const *, Slice const *);

#endif
