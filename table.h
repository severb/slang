#ifndef clox_table_h
#define clox_table_h

#include "val.h"

typedef struct {
  Val key;
  Val val;
} Entry;

typedef struct Table {
  size_t len;
  size_t cap;
  Entry *entries;
} Table;

Table *table_init(Table *);
void table_destroy(Table *);

#define SET_NEW usr_val_(0)
#define SET_OVERRIDE usr_val_(1)

// table_set() returns an error Val if the table cannot grow or a user symbol
// (either SET_NEW or SET_OVERRIDE) to indicate if the key/value pair is new.
Val table_set(Table *, Val key, Val);

Val table_setdefault(Table *, Val key, Val);
Val table_get(const Table *, Val key);
Val table_pop(Table *, Val key);

typedef struct {
  Entry *entries;
  size_t cap;
  size_t idx;
} TableIter;

// tableiter_init() keeps a pointer to the Table's entries which makes mutating
// the map during the iteration unsafe.
TableIter *tableiter_init(TableIter *, const Table *);
Entry *tableiter_next(TableIter *);

#endif
