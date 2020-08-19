#ifndef slang_table_h
#define slang_table_h

#include "dynarray.h" // dynarray_*
#include "tag.h"      // Tag

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
void table_destroy(Table *);
void table_free(Table *);
void table_printf(FILE *, const Table *);
inline void table_print(const Table *t) { table_printf(stdout, t); }
bool table_set(Table *, Tag key, Tag val);
bool table_get(const Table *, Tag key, Tag *val);
bool table_del(Table *, Tag);

inline size_t table_len(const Table *t) { return t->real_len; }

// SLANG_DEBUG
typedef struct TableStats {
    uint64_t queries;
    uint64_t collisions;
} TableStats;

void table_print_summary(const Table *);
TableStats table_stats(void);

#endif
