#include "table.h"

#include "err.h"     // ERROR
#include "listgen.h" // LIST_IMPL
#include "val.h"     // Val, val_biteq, usr_val_, val_destroy, val_hash, ref

#include <assert.h>  // assert
#include <stdbool.h> // bool, uint32_t
#include <stdio.h>   // fptus, stderr
#include <stdlib.h>  // abort, size_t

#define TABLE_MAX_LOAD(n) (((n) / 4) * 3)
#define UNSET usr_val_(0)
#define TOMBSTONE usr_val_(1)

#define EXTRA_DESTROY                                                          \
  TableIter iter;                                                              \
  tableiter_init(&iter, list);                                                 \
  Entry *entry;                                                                \
  while ((entry = tableiter_next(&iter))) {                                    \
    val_destroy(entry->key);                                                   \
    val_destroy(entry->val);                                                   \
  }

LIST_IMPL(entry, Entry)

#undef EXTRA_DESTROY

Val err_key = ERROR(key not found);

static inline bool is_tombstone(Val v) { return val_biteq(v, TOMBSTONE); }
static inline bool is_unset(Val v) {
  return val_biteq(v, UNSET) || is_tombstone(v);
}

TableIter *tableiter_init(TableIter *iter, const Table *table) {
  if (iter != 0)
    *iter = (TableIter){
        .table = table,
        .idx = 0,
    };
  return iter;
}

Entry *tableiter_next(TableIter *iter) {
  while (iter->idx < iter->table->cap) {
    size_t i = iter->idx;
    iter->idx++;
    Entry *entry = &iter->table->vals[i];
    if (is_unset(entry->key))
      continue;
    return entry;
  }
  return 0;
}

static Entry *table_find_entry(const Table *table, Val key) {
  // TODO: hash tables can't grow as large as lists because hashes are only 32b
  uint32_t idx = val_hash(key) % table->cap;
  Entry *frst_tmbstone = 0;
  for (;;) {
    Entry *entry = &table->vals[idx];
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
    idx = (idx + 1) % table->cap; // OK to wrap-around on addition
  }
}

#define UNSET_ENTRY                                                            \
  (Entry) { UNSET, NIL }

static inline void rehash(Table *table, size_t i) {
  Val key = table->vals[i].key;
  if (val_biteq(key, UNSET))
    return;
  if (is_tombstone(key)) {
    table->vals[i].key = UNSET; // continue to convert tobstones to unsets
    table->len--;
    return;
  }
  Val val = table->vals[i].val;
  table->vals[i] = (Entry){UNSET, NIL};
  Entry *e = table_find_entry(table, key);
  e->key = key;
  e->val = val;
}

// table_grow() grows the list and re-hashes the entries inline.
static void table_grow(Table *table) {
  size_t old_cap = table->cap;
  list_entry_grow(table);
  // make new entries UNSET
  for (size_t i = old_cap; i < table->cap; i++) {
    table->vals[i] = UNSET_ENTRY;
  }
  // start is the index of the 1st entry preceded by at least one UNSET and
  // zero or more TOMBSTONEs. This entry and all others to its right have
  // their hash pos >= than their idx, property needed for inline rehashing.
  size_t start = 0;
  bool unset_found = false;
  // find the start position
  while (table->len) {
    Val key = table->vals[start].key;
    unset_found = unset_found || val_biteq(key, UNSET);
    if (unset_found) {
      if (is_tombstone(key)) {
        // convert any TOMBSTONEs between the last UNSET and the first entry
        table->vals[start].key = UNSET;
        table->len--;
      } else {
        break; // found it
      }
    }
    start = (start + 1) % old_cap; // OK to wrap-around on addition
  }
  if (!table->len)
    return;
  for (size_t i = start; i < old_cap; i++) {
    rehash(table, i);
  }
  for (size_t i = 0; i < start; i++) {
    rehash(table, i);
  }
}

Val table_setorget(Table *table, Val key, Val val) {
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

Val table_get(Table const *table, Val key) {
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
  entry->key = TOMBSTONE; // tombstone is unset
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
