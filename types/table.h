#ifndef slang_table_h
#define slang_table_h

#include "types/dynarray.h" // dynarray_*
#include "types/tag.h"      // Tag

#include <stdbool.h> // bool
#include <stddef.h>  // size_t
#include <stdint.h>  // uint64_t

typedef struct Entry {
  Tag key;
  Tag val;
} Entry;

dynarray_declare(Entry);
typedef struct Table {
  DynamicArray(Entry) array;
  size_t real_len;
} Table;

bool table_eq(const Table *, const Table *);
void table_free(Table *);
void table_print(const Table *);
size_t table_set(Table *, Tag key, Tag val);
bool table_get(const Table *, Tag key, Tag *val);
bool table_del(Table *, Tag);

inline size_t table_len(const Table *t) {
  return dynarray_len(Entry)(&t->array);
}

// SLANG_DEBUG
typedef struct TableStats {
  uint64_t queries;
  uint64_t collisions;
} TableStats;

void table_print_summary(const Table *);
TableStats table_stats(void);

#endif
