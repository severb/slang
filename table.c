#include <assert.h>
#include <stdio.h>

#include "common.h"
#include "mem.h"
#include "table.h"
#include "val.h"

#define MAX_LOAD .75

#define UNSET VAL_LIT_INVALID(false)
#define TOMBSTONE VAL_LIT_INVALID(true)

static bool is_unset(Val v) { return VAL_IS_INVALID(v); }
static bool is_tombstone(Val v) { return v.val.as.boolean == true; }

TableIter *tableiter_init(TableIter *iter, const Table *table) {
  if (iter == 0)
    return 0;
  *iter = (TableIter){
      .entries = table->entries,
      .cap = table->cap,
      .idx = 0,
  };
  return iter;
}

Entry *tableiter_next(TableIter *iter) {
  while (iter->idx < iter->cap) {
    Entry *entry = &iter->entries[iter->idx];
    iter->idx++;
    if (is_unset(entry->key))
      continue;
    return entry;
  }
  return 0;
}

Table *table_init(Table *table) {
  if (table == 0)
    return 0;
  *table = (Table){0};
  return table;
}

void table_destroy(Table *table) {
  if (table == 0)
    return;
  TableIter iter;
  tableiter_init(&iter, table);
  Entry *entry;
  while ((entry = tableiter_next(&iter))) {
    val_destroy(&entry->key);
    val_destroy(&entry->val);
  }
  FREE_ARRAY(table->entries, Entry, table->cap);
  table_init(table);
}

static void table_grow(Table *table) {
  TableIter iter;
  tableiter_init(&iter, table);
  table->cap = GROW_CAPACITY(table->cap);
  table->entries = GROW_ARRAY(0, Entry, 0, table->cap);
  table->len = 0;
  for (int i = 0; i < table->cap; i++) {
    Entry *entry = &table->entries[i];
    entry->key = UNSET; // unset but not tombstone
    entry->val = VAL_LIT_NIL;
  };
  Entry *entry;
  while ((entry = tableiter_next(&iter))) {
    table_set(table, &entry->key, &entry->val);
  }
  FREE_ARRAY(iter.entries, Entry, iter.cap);
}

static Entry *table_find_entry(const Table *table, Val key) {
  int idx = val_hash(key) % table->cap;
  Entry *frst_tmbstone = 0;
  for (;;) {
    Entry *entry = &table->entries[idx];
    if (is_unset(entry->key)) {
      if (is_tombstone(entry->key)) {
        // tombstone here
        frst_tmbstone = frst_tmbstone != 0 ? frst_tmbstone : entry;
      } else {
        return frst_tmbstone != 0 ? frst_tmbstone : entry;
      }
    } else {
      if (val_equals(entry->key, key))
        return entry;
    }
    idx = (idx + 1) % table->cap;
  }
}

Val *table_setdefault(Table *table, Val *key, Val *val) {
  if (table->len + 1 >= table->cap * MAX_LOAD)
    table_grow(table);
  Entry *entry = table_find_entry(table, *key);
  bool is_new = is_unset(entry->key);
  if (is_new) {
    if (!is_tombstone(entry->key)) {
      table->len++; // inc len only when !tombstone
    }
    entry->key = *key;
    *key = VAL_LIT_NIL;
  } else {
    val_destroy(key);
    val_destroy(val);
    return &entry->val;
  }
  entry->val = *val;
  *val = VAL_LIT_NIL;
  return &entry->val;
}

bool table_set(Table *table, Val *key, Val *val) {
  if (table->len + 1 >= table->cap * MAX_LOAD)
    table_grow(table);
  Entry *entry = table_find_entry(table, *key);
  bool is_new = is_unset(entry->key);
  if (is_new) {
    if (!is_tombstone(entry->key)) {
      table->len++; // inc len only when !tombstone
    }
    entry->key = *key;
    *key = VAL_LIT_NIL;
  } else {
    val_destroy(&entry->val);
    val_destroy(key);
  }
  entry->val = *val;
  *val = VAL_LIT_NIL;
  return is_new;
}

Val *table_get(const Table *table, Val key) {
  if (table->len == 0)
    return 0;
  Entry *entry = table_find_entry(table, key);
  if (is_unset(entry->key))
    return 0;
  return &entry->val;
}

bool table_del(Table *table, Val key) {
  if (table->len == 0)
    return false;
  Entry *entry = table_find_entry(table, key);
  if (is_unset(entry->key))
    return false;
  val_destroy(&entry->key);
  entry->key = TOMBSTONE; // unset and tombstone
  val_destroy(&entry->val);
  return true;
}

void table_copy_safe(const Table *from, Table *to) {
  for (int i = 0; i < from->cap; i++) {
    Entry *entry = &from->entries[i];
    if (is_unset(entry->key))
      continue;
    Val key = entry->key;
    if (VAL_IS_STR(key))
      key = VAL_LIT_SLICE(str_slice(key.val.as.str));
    Val val = entry->val;
    if (VAL_IS_STR(val))
      val = VAL_LIT_SLICE(str_slice(val.val.as.str));
    table_set(to, &key, &val);
  }
}
