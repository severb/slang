#include "types/table.h"

#include "types/dynarray.h" // dynarray_*
#include "types/tag.h"      // tag_biteq

#include <stdbool.h> // bool, true, false
#include <stddef.h>  // size_t
#include <stdio.h>   // putchar, puts

#define TOMBSTONE_KEY USER_SYMBOL(0)
#define EMPTY_KEY USER_SYMBOL(1)

#ifdef SLANG_DEBUG

static TableStats stats = {0};

TableStats table_stats(void) { return stats; }

void table_print_summary(const Table *t) {
  size_t cap = dynarray_cap(Entry)(&t->array);
  for (size_t i = 0; i < cap; i++) {
    Entry *entry = dynarray_get(Entry)(&t->array, i);
    if (tag_biteq(entry->key, TOMBSTONE_KEY)) {
      putchar('.');
    } else if (tag_biteq(entry->key, TOMBSTONE_KEY)) {
      putchar(' ');
    } else {
      putchar('#');
    }
  }
  putchar('\n');
}

#endif

static bool key_eq(Tag a, Tag b) {
  switch (tag_type(a)) {
  case TYPE_TABLE:
  case TYPE_LIST:
    return tag_is_ptr(b) && tag_to_ptr(a) == tag_to_ptr(b);
  case TYPE_ERROR:
    return tag_is_error(b) && key_eq(*tag_to_error(a), *tag_to_error(b));
  default:
    return tag_eq(a, b);
  }
}

static bool is_unset(Tag t) {
  return tag_biteq(t, EMPTY_KEY) || tag_biteq(t, TOMBSTONE_KEY);
}

static Entry *find_entry(const Table *t, Tag key) {
#ifdef SLANG_DEBUG
  stats.queries++;
#endif
  size_t hash = tag_hash(key);
  size_t cap = dynarray_cap(Entry)(&t->array);
  assert((cap & (cap - 1)) == 0 && "cap not a power of two");
  size_t mask = cap - 1;
  size_t idx = hash & mask;
  Entry *first_tombstone = 0;
  for (;;) {
    Entry *entry = dynarray_get(Entry)(&t->array, idx);
    if (tag_biteq(entry->key, EMPTY_KEY)) {
      return first_tombstone ? first_tombstone : entry;
    } else if (tag_biteq(entry->key, TOMBSTONE_KEY)) {
      first_tombstone = first_tombstone ? first_tombstone : entry;
    } else {
      if (key_eq(entry->key, key)) {
        return entry;
#ifdef SLANG_DEBUG
      } else {
        stats.collisions++;
#endif
      }
    }
    idx = (idx + 1) & mask;
  }
}

static size_t grow(Table *t) {
  size_t old_cap = dynarray_cap(Entry)(&t->array);
  size_t new_cap = dynarray_grow(Entry)(&t->array);
  if (!new_cap) {
    return 0;
  }
  for (size_t i = old_cap; i < new_cap; i++) {
    dynarray_get(Entry)(&t->array, i)->key = EMPTY_KEY;
  }
  if (!old_cap) {
    return new_cap;
  }

  // Inline rehashing: find the 1st unset and and start rehashing entries to its
  // right which should either move between the 1st unset and their position or
  // to the new zone. Continue rehashing entries to the right. Then, rehash the
  // items before the 1st unset.
  size_t start = 0;
  // find the start position
  for (start = 0; start < old_cap; start++) {
    if (tag_biteq(dynarray_get(Entry)(&t->array, start)->key, EMPTY_KEY)) {
      break;
    }
  }
  assert((start < old_cap || old_cap == 0) && "table can't be full");
  // reset tombstones
  for (size_t i = 0; dynarray_len(Entry)(&t->array) > t->real_len; i++) {
    Entry *entry = dynarray_get(Entry)(&t->array, i);
    if (tag_biteq(entry->key, TOMBSTONE_KEY)) {
      entry->key = EMPTY_KEY;
      dynarray_trunc(Entry)(&t->array, dynarray_len(Entry)(&t->array) - 1);
    }
  }
  assert(dynarray_len(Entry)(&t->array) == t->real_len && "lens don't match");
  // rehash
  size_t remaining = t->real_len;
  assert((old_cap & (old_cap - 1)) == 0 && "cap not a power of two");
  size_t mask = old_cap - 1;
  for (size_t i = (start + 1) & mask; remaining; i = (i + 1) & mask) {
    Entry *entry = dynarray_get(Entry)(&t->array, i);
    if (tag_biteq(entry->key, EMPTY_KEY)) {
      continue;
    }
    remaining--;
    Entry copy = *entry;
    entry->key = EMPTY_KEY;
    *find_entry(t, copy.key) = copy;
  }
  return new_cap;
}

size_t table_set(Table *t, Tag key, Tag val) {
  assert(!is_unset(key) && "table keys cannot be user symbols 0 or 1");
  size_t len = dynarray_len(Entry)(&t->array);
  size_t cap = dynarray_cap(Entry)(&t->array);
  assert((len == 0 || len < cap) && "table invariant");
  assert(len >= t->real_len && "table invariant");
  if (len + 1 > (cap / 7) * 5) {
    if (!grow(t)) {
      return 0;
    }
    len = dynarray_len(Entry)(&t->array); // len changes when tombstones clear
  }
  Entry *entry = find_entry(t, key);
  if (tag_biteq(entry->key, TOMBSTONE_KEY)) {
    entry->key = key;
    t->real_len++;
  } else if (tag_biteq(entry->key, EMPTY_KEY)) {
    entry->key = key;
    dynarray_trunc(Entry)(&t->array, len + 1);
    t->real_len++;
  } else {
    tag_free(entry->val);
    tag_free(key);
  }
  entry->val = val;
  return t->real_len;
}

bool table_get(const Table *t, Tag key, Tag *val) {
  assert(!is_unset(key) && "table keys cannot be user symbols 0 or 1");
  if (t->real_len == 0) {
    return false;
  }
  Entry *entry = find_entry(t, key);
  if (is_unset(entry->key)) {
    return false;
  }
  if (val) {
    *val = entry->val;
  }
  return true;
}

bool table_del(Table *t, Tag key) {
  assert(!is_unset(key) && "table keys cannot be user symbols 0 or 1");
  if (t->real_len == 0) {
    return false;
  }
  Entry *entry = find_entry(t, key);
  if (is_unset(entry->key)) {
    return false;
  }
  tag_free(entry->key);
  tag_free(entry->val);
  entry->key = TOMBSTONE_KEY;
  t->real_len--;
  return true;
}

void table_free(Table *t) {
  assert(t->real_len < dynarray_cap(Entry)(&t->array) || t->real_len == 0);
  size_t remaining = t->real_len;
  for (size_t i = 0; remaining; i++) {
    Entry *entry = dynarray_get(Entry)(&t->array, i);
    if (is_unset(entry->key)) {
      continue;
    }
    remaining--;
    tag_free(entry->key);
    tag_free(entry->val);
  }
  dynarray_free(Entry)(&t->array);
  t->real_len = 0;
}

void table_print(const Table *t) {
  assert(t->real_len < dynarray_cap(Entry)(&t->array) || t->real_len == 0);
  putchar('{');
  size_t remaining = t->real_len;
  for (size_t i = 0; remaining; i++) {
    Entry *entry = dynarray_get(Entry)(&t->array, i);
    if (is_unset(entry->key)) {
      continue;
    }
    remaining--;
    tag_print(entry->key);
    puts(": ");
    tag_print(entry->val);
    if (remaining > 1) {
      puts(", ");
    }
  }
  putchar('}');
}

bool table_eq(const Table *a, const Table *b) {
  assert(a->real_len < dynarray_cap(Entry)(&a->array) || a->real_len == 0);
  assert(b->real_len < dynarray_cap(Entry)(&b->array) || b->real_len == 0);
  if (a->real_len != b->real_len) {
    return false;
  }
  size_t remaining = a->real_len;
  for (size_t i = 0; remaining; i++) {
    Entry *entry = dynarray_get(Entry)(&a->array, i);
    if (is_unset(entry->key)) {
      continue;
    }
    remaining--;
    Tag val;
    if (!table_get(b, entry->key, &val)) {
      return false;
    }
    if (!tag_eq(entry->val, val)) {
      return false;
    }
  }
  return true;
}

extern inline size_t table_len(const Table *);
