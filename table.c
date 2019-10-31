#include "table.h"

#include "err.h" // ERROR
#include "mem.h" // FREE_ARRAY, GROW_ARRAY
#include "val.h" // Val, val_biteq, usr_val_, val_destroy, val_hash, ref

#include <assert.h>  // assert
#include <stdbool.h> // bool
#include <stdio.h>   // fptus, stderr
#include <stdlib.h>  // abort, size_t

#define TABLE_MAX_LOAD(n) (((n) / 4) * 3)
#define UNSET usr_val_(0)
#define TOMBSTONE usr_val_(1)

Val err_key = ERROR(key not found);

static inline bool is_tombstone(Val v) { return val_biteq(v, TOMBSTONE); }
static inline bool is_unset(Val v) {
  return val_biteq(v, UNSET) || is_tombstone(v);
}

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
    size_t i = iter->idx;
    iter->idx++;
    Entry *entry = &iter->entries[i];
    if (is_unset(entry->key))
      continue;
    return entry;
  }
  return 0;
}

Table *table_init(Table *table) {
  if (table == 0)
    return 0;
  *table = (Table){.len = 0, .cap = 0, .entries = 0};
  return table;
}

void table_destroy(Table *table) {
  if (table == 0)
    return;
  TableIter iter;
  tableiter_init(&iter, table);
  Entry *entry;
  while ((entry = tableiter_next(&iter))) {
    val_destroy(entry->key);
    val_destroy(entry->val);
  }
  FREE_ARRAY(table->entries, Entry, table->cap);
  table_init(table);
}

static void table_grow(Table *table) {
  // NB: instead of reallocating the entries array, allocate a new one ond
  // rehash the entries from the old array
  TableIter iter;
  tableiter_init(&iter, table);
  if (table->cap < 8) {
    table->cap = 8;
  } else if (table->cap <= (SIZE_MAX / 2 / sizeof(Entry))) {
    table->cap *= 2;
  } else {
    fputs("table exceeds its maximum capacity", stderr);
    abort();
  }
  table->entries = GROW_ARRAY(0, Entry, 0, table->cap);
  if (!table->entries) {
    fputs("not enough memory to grow the table", stderr);
    abort();
  }
  table->len = 0;
  for (size_t i = 0; i < table->cap; i++) {
    // unset, but not tombstone
    table->entries[i].key = UNSET;
  };
  Entry *entry;
  while ((entry = tableiter_next(&iter))) {
    table_set(table, entry->key, entry->val);
  }
  if (iter.cap) {
    FREE_ARRAY(iter.entries, Entry, iter.cap);
  }
}

static Entry *table_find_entry(const Table *table, Val key) {
  int idx = val_hash(key) % table->cap;
  Entry *frst_tmbstone = 0;
  for (;;) {
    Entry *entry = &table->entries[idx];
    if (is_unset(entry->key)) {
      if (is_tombstone(entry->key)) {
        frst_tmbstone = frst_tmbstone != 0 ? frst_tmbstone : entry;
      } else {
        return frst_tmbstone != 0 ? frst_tmbstone : entry;
      }
    } else {
      if (val_equals(entry->key, key)) {
        return entry;
      }
    }
    idx = (idx + 1) % table->cap;
  }
}

Val table_setorgetref(Table *table, Val key, Val val) {
  assert(!is_usr_symbol(key));
  assert(table->len == 0 || table->len < table->cap);
  if (table->len + 1 > TABLE_MAX_LOAD(table->cap)) {
    table_grow(table);
  }
  Entry *entry = table_find_entry(table, key);
  bool is_new = is_unset(entry->key);
  if (is_new) {
    if (!is_tombstone(entry->key)) {
      table->len++; // inc len only when not tombstone
    }
    entry->key = key;
    entry->val = val;
  } else {
    val_destroy(key);
    val_destroy(val);
  }
  return ref(entry->val);
}

bool table_set(Table *table, Val key, Val val) {
  assert(!is_usr_symbol(key));
  assert(table->len == 0 || table->len < table->cap);
  if (table->len + 1 > TABLE_MAX_LOAD(table->cap)) {
    table_grow(table);
  }
  Entry *entry = table_find_entry(table, key);
  bool is_new = is_unset(entry->key);
  if (is_new) {
    if (!is_tombstone(entry->key)) {
      table->len++; // inc len only when not tombstone
    }
    entry->key = key;
  } else {
    val_destroy(entry->val);
    val_destroy(key); // keep the old key
  }
  entry->val = val;
  return is_new;
}

Val table_getref(const Table *table, Val key) {
  assert(!is_usr_symbol(key));
  if (table->len == 0) {
    return err_key;
  }
  Entry *entry = table_find_entry(table, key);
  if (is_unset(entry->key)) {
    return err_key;
  }
  return ref(entry->val);
}

Val table_pop(Table *table, Val key) {
  assert(!is_usr_symbol(key));
  if (table->len == 0) {
    return err_key;
  }
  Entry *entry = table_find_entry(table, key);
  if (is_unset(entry->key)) {
    return err_key;
  }
  Val res = entry->val;
  val_destroy(entry->key);
  entry->key = TOMBSTONE; // unset and tombstone
  return res;
}

bool table_del(Table *table, Val key) {
  Val v = table_pop(table, key);
  bool found = !val_biteq(v, err_key);
  if (found) {
    val_destroy(v);
  }
  return found;
}
