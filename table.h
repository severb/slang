#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "val.h"

typedef struct {
  Val key;
  Val val;
} Entry;

typedef struct sTable {
  size_t len;
  size_t cap;
  Entry *entries;
} Table;

Table *table_init(Table *);
void table_destroy(Table *);

bool table_set(Table *, Val *key, Val *);
Val *table_setdefault(Table *, Val *key, Val *);
Val *table_get(const Table *, Val key);
bool table_del(Table *, Val key);
void table_copy_safe(const Table *from, Table *to);

typedef struct {
  Entry *entries;
  size_t cap;
  size_t idx;
} TableIter;

TableIter *tableiter_init(TableIter *, const Table *);
Entry *tableiter_next(TableIter *);

#endif
