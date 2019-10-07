#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "object.h"
#include "value.h"

typedef struct {
  ObjStringBase *key;
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

bool tableSet(Table *, ObjStringBase *key, Value);
bool tableGet(Table *, ObjStringBase *key, Value *);
bool tableDel(Table *, ObjStringBase *key);
void tableCopy(Table *from, Table *to);

void tableIterInit(TableIter *iter, Table *table);
ObjStringBase *tableIterNextKey(TableIter *iter);

ObjStringBase *internObjStringBase(Table *, ObjStringBase *);
ObjStringBase *internObjStringStatic(Table *, const char *, int length);
ObjStringBase *internObjString(Table *, const char *, int length);

#endif
