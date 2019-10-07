#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "object.h"
#include "value.h"

typedef struct {
  Obj *key;
  Value value;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry *entries;
} Table;

typedef struct {
  Table *table;
  int idx;
} TableIter;

void tableInit(Table *table);
void tableFree(Table *table);
void tableFreeKeys(Table *table);

bool tableSet(Table *table, Obj *key, Value value);
bool tableGet(Table *table, Obj *key, Value *value);
bool tableDel(Table *table, Obj *key);
void tableCopy(Table *from, Table *to);

void tableIterInit(TableIter *iter, Table *table);
Obj *tableIterNextKey(TableIter *iter);

Obj *internObj(Table *table, Obj *obj);
Obj *internObjStringStatic(Table *table, const char *start, int length);
Obj *internObjString(Table *table, const char *start, int length);
Obj *internStringsConcat(Table *table, Obj *a, Obj *b);

#endif
