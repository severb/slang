#include <assert.h>
#include <stdio.h>

#include "common.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define MAX_LOAD .70

#define EMPTY() VAL_LIT_BOOL(false)
#define TOMBSTONE() VAL_LIT_BOOL(true)

static bool isTombstone(Value value) {
  return VAL_IS_BOOL(value) && VAL_AS_BOOL(value);
}

void tableIterInit(TableIter *iter, Table *table) {
  iter->table = table;
  iter->idx = 0;
}

ObjStringBase *tableIterNextKey(TableIter *iter) {
  while (iter->idx < iter->table->capacity) {
    Entry *entry = &iter->table->entries[iter->idx];
    iter->idx++;
    if (entry->key == NULL || isTombstone(entry->value)) {
      continue;
    }
    return entry->key;
  }
  return NULL;
}

void tableInit(Table *table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void tableFree(Table *table) {
  FREE_ARRAY(table->entries, Entry, table->capacity);
  tableInit(table);
}

void tableFreeKeys(Table *table) {
  TableIter i;
  tableIterInit(&i, table);
  ObjStringBase *obj;
  while ((obj = tableIterNextKey(&i)) != NULL) {
    objFree((Obj *)obj);
  }
}

static void tableGrow(Table *table) {
  int oldCapacity = table->capacity;
  Entry *oldEntries = table->entries;
  table->capacity = GROW_CAPACITY(oldCapacity);
  table->entries = GROW_ARRAY(NULL, Entry, 0, table->capacity);
  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    table->entries[i].key = NULL;
    table->entries[i].value = EMPTY();
  }
  for (int i = 0; i < oldCapacity; i++) {
    Entry e = oldEntries[i];
    if (e.key != NULL)
      tableSet(table, e.key, e.value);
  }
  FREE_ARRAY(oldEntries, Entry, oldCapacity);
}

static Entry *tableFindEntry(Table *table, ObjStringBase *key) {
  int idx = objHash(key) % table->capacity;
  Entry *firstTombstone = NULL;
  for (;;) {
    Entry *entry = &table->entries[idx];
    if (entry->key == NULL) {
      if (isTombstone(entry->value)) { // remember the 1st tombstone
        firstTombstone = firstTombstone != NULL ? firstTombstone : entry;
      } else { // not found, return the 1st tombstone if exists
        return firstTombstone != NULL ? firstTombstone : entry;
      }
    } else if (objsEqual((Obj *)entry->key, (Obj *)key))
      return entry;
    idx = (idx + 1) % table->capacity;
  }
}

bool tableSet(Table *table, ObjStringBase *key, Value value) {
  if (table->count + 1 >= table->capacity * MAX_LOAD)
    tableGrow(table);

  Entry *entry = tableFindEntry(table, key);

  bool isNewKey = (entry->key == NULL || isTombstone(entry->value));
  if (isNewKey) {
    table->count++;
    entry->key = key;
  }

  entry->value = value;
  return isNewKey;
}

bool tableGet(Table *table, ObjStringBase *key, Value *value) {
  if (table->count == 0)
    return false;

  Entry *entry = tableFindEntry(table, key);
  if (entry->key == NULL)
    return false;

  *value = entry->value;
  return true;
}

bool tableDel(Table *table, ObjStringBase *key) {
  if (table->count == 0)
    return false;

  Entry *entry = tableFindEntry(table, key);
  if (entry->key == NULL)
    return false;

  entry->key = NULL;
  entry->value = TOMBSTONE();
  return true;
}

void tableCopy(Table *from, Table *to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry *entry = &from->entries[i];
    if (entry->key != NULL)
      tableSet(to, entry->key, entry->value);
  }
}

ObjStringBase *internObjStringBase(Table *table, ObjStringBase *obj) {
  Value value;
  if (tableGet(table, obj, &value)) {
    objFree((Obj *)obj);
    return OBJ_AS_OBJSTRINGBASE(VAL_AS_OBJ(value));
  }
  tableSet(table, obj, VAL_LIT_OBJ(obj));
  return obj;
}

ObjStringBase *internObjStringStatic(Table *table, const char *start,
                                     int length) {
  ObjStringStatic *obj = objStringStaticNew(start, length);
  return internObjStringBase(table, (ObjStringBase *)obj);
}

ObjStringBase *internObjString(Table *table, const char *start, int length) {
  ObjString *obj = objStringNew(start, length);
  return internObjStringBase(table, (ObjStringBase *)obj);
}
