#include "table.h"

#include "mem.h" // FREE_ARRAY, GROW_ARRAY
#include "val.h" // BYTES, SYMB_DATA_TYPE, Val, val_u, ref

#include <assert.h> // assert

#define TABLE_MAX_LOAD(n) ((n / 4) * 3)

#define UNSET usr_val_(100)
#define TOMBSTONE usr_val_(200)

#define is_unset_(v) (val_biteq_(v, UNSET) || val_biteq_(v, TOMBSTONE))
#define is_tombstone_(v) (val_biteq_(v, TOMBSTONE))

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
    if (is_unset_(entry->key))
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

static bool table_grow(Table *table) {
  // NB: instead of reallocating the entries array, allocate a new array ond
  // rehash the entries from the old array
  TableIter iter;
  tableiter_init(&iter, table);
  if (table->cap < 8) {
    table->cap = 8;
  } else if (table->cap <= (SIZE_MAX / 2 / sizeof(Entry))) {
    table->cap *= 2;
  } else {
    return false;
  }
  table->entries = GROW_ARRAY(0, Entry, 0, table->cap);
  table->len = 0;
  for (size_t i = 0; i < table->cap; i++) {
    Entry *entry = &table->entries[i];
    // unset but not tombstone
    entry->key = UNSET;
    entry->val = UNSET;
  };
  Entry *entry;
  while ((entry = tableiter_next(&iter))) {
    table_set(table, entry->key, entry->val);
  }
  FREE_ARRAY(iter.entries, Entry, iter.cap);
  return true;
}

static Entry *table_find_entry(const Table *table, Val key) {
  int idx = val_hash(key) % table->cap;
  Entry *frst_tmbstone = 0;
  for (;;) {
    Entry *entry = &table->entries[idx];
    if (is_unset_(entry->key)) {
      if (is_tombstone_(entry->key)) {
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
  Val err_grow = UNSET;
  // DECL_STATIC_ERR(err_grow, "cannot grow the table")
  if (table->len + 1 > TABLE_MAX_LOAD(table->cap)) {
    if (!table_grow(table)) {
      return err_grow;
    }
  }
  Entry *entry = table_find_entry(table, key);
  bool is_new = is_unset_(entry->key);
  if (is_new) {
    if (!is_tombstone_(entry->key)) {
      table->len++; // inc len only when !tombstone
    }
    entry->key = key;
    entry->val = val;
  } else {
    val_destroy(key);
    val_destroy(val);
  }
  return ref(entry->val);
}

Val table_set(Table *table, Val key, Val val) {
  assert(!is_usr_symbol(key));
  assert(table->len == 0 || table->len < table->cap);
  Val err_grow = UNSET;
  // DECL_STATIC_ERR(err_grow, "cannot grow the table")
  if (table->len + 1 > TABLE_MAX_LOAD(table->cap)) {
    if (!table_grow(table)) {
      return err_grow;
    }
  }
  Entry *entry = table_find_entry(table, key);
  bool is_new = is_unset_(entry->key);
  if (is_new) {
    if (!is_tombstone_(entry->key)) {
      table->len++; // inc len only when !tombstone
    }
    entry->key = key;
  } else {
    val_destroy(entry->val);
    val_destroy(key); // keep the old key
  }
  entry->val = val;
  return is_new ? SET_NEW : SET_OVERRIDE;
}

Val table_getref(const Table *table, Val key) {
  assert(!is_usr_symbol(key));
  Val err_empty_table = UNSET;
  Val err_bad_key = UNSET;
  // DECL_STATIC_ERR(err_empty_table, "getref on empty table")
  // DECL_STATIC_ERR(err_bad_key, "bad key")
  if (table->len == 0)
    return err_empty_table;
  Entry *entry = table_find_entry(table, key);
  if (is_unset_(entry->key))
    return err_bad_key;
  return ref(entry->val);
}

Val table_pop(Table *table, Val key) {
  assert(!is_usr_symbol(key));
  Val err_empty_table = UNSET;
  Val err_bad_key = UNSET;
  Val err_unset_val = UNSET;
  // DECL_STATIC_ERR(err_empty_table, "pop on empty table")
  // DECL_STATIC_ERR(err_bad_key, "bad key")
  // DECL_STATIC_ERR(err_unset_val, "unset value")
  if (table->len == 0)
    return err_empty_table;
  Entry *entry = table_find_entry(table, key);
  if (is_unset_(entry->key))
    return err_bad_key;
  Val res = entry->val;
  val_destroy(entry->key);
  entry->key = TOMBSTONE; // unset and tombstone
  entry->val = err_unset_val;
  return res;
}
