#ifndef clox_table_h
#define clox_table_h

#include "val.h" // Val

#include "listgen.h" // LIST_DECL

#include <stdbool.h> // bool
#include <stdlib.h>  // size_t

typedef struct {
  Val key;
  Val val;
} Entry;

LIST_DECL(entry, Entry)

typedef List_entry Table;

#define table_init(t) list_entry_init(t)
#define table_destroy(t) list_entry_destroy(t)

// table_set() sets the key and returns true if the pair is new.
bool table_set(Table *, Val key, Val);
Val table_setorget(Table *, Val key, Val);

// key_err is returned when the key is not found and can be used with val_biteq
// to check if the operation was successful or not.
extern Val err_key;

Val table_get(Table const *, Val key);
Val table_pop(Table *, Val key);
bool table_del(Table *, Val key);

typedef struct {
  Table const *table;
  size_t idx;
} TableIter;

// tableiter_init() keeps a pointer to the Table's entries which makes mutating
// the map during the iteration unsafe.
TableIter *tableiter_init(TableIter *, const Table *);
Entry *tableiter_next(TableIter *);

#endif
