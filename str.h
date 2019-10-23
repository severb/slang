#ifndef clox_str_h
#define clox_str_h

#include "common.h"

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

uint32_t str_hash(const char *, size_t len);

Str *str_new(const char *, uint32_t len);
void str_free(Str *);
Str *str_copy(Str *);
Slice str_slice(Str *);

Slice *slice_init(Slice *, const char *, uint32_t len);
Str *slice_str(Slice);
void slice_print(Slice);
bool slice_equals(Slice, Slice);
Str *slice_concat(Slice, Slice);

#endif
