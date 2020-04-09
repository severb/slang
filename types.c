#include "types.h"

#include "mem.h" // mem_allocate_flex, mem_free_array, mem_resize_array
#include "val.h" // val_free, val_print

#include <inttypes.h> // PRId*
#include <stdint.h>   // uint64_t, UINT64_C
#include <stdio.h>    // printf
#include <stdlib.h>   // abort
#include <string.h>   // memcpy, size_t

arraylist_define(Val);   // List
arraylist_define(Entry); // Table

String *string_new_empty(size_t len) {
  String *res = mem_allocate_flex(sizeof(String), sizeof(char), len);
  if (res == 0) {
    return 0;
  }
  res->hash = 0;
  res->len = len;
  return res;
}

String *string_new(char const *c, size_t len) {
  String *res = string_new_empty(len);
  if (res == 0) {
    return 0;
  }
  memcpy(res->c, c, len);
  return res;
}

size_t chars_hash(const char *c, size_t len) {
  uint64_t res = UINT64_C(2166136261);
  for (size_t i = 0; i < len; i++) {
    res ^= c[i];
    res *= UINT64_C(16777619);
  }
  if (res == 0) { // avoid zero-hashes because 0 indicates no hash is cached
    res = UINT64_C(0x1337);
  }
  return (size_t)res;
}

void list_free(List *l) {
  size_t len = arraylist_len(Val)(&l->al);
  for (size_t i = 0; i < len; i++) {
    val_free(*arraylist_get(Val)(&l->al, i));
  }
  arraylist_free(Val)(&l->al);
}

void list_print(const List *l) {
  printf("[");
  size_t len = arraylist_len(Val)(&l->al);
  for (size_t i = 0; i < len; i++) {
    val_print(*arraylist_get(Val)(&l->al, i));
    if (i + 1 < len) {
      printf(", ");
    }
  }
  printf("]");
}

bool list_eq(const List *a, const List *b) { return true; }

#define EMPTY_ITEM USR_SYMBOL(0)
#define TOMBSTONE_ITEM USR_SYMBOL(1)

static bool key_eq(Val a, Val b) {
  switch (val_type(a)) {
  case VAL_TABLE:
  case VAL_LIST:
    // compare lists and tables by identity, not element by element
    return val_is_ptr(b) && val_ptr2ptr(a) == val_ptr2ptr(b);
  case VAL_ERR:
    // avoid comparing lists and tables element by element when inside errors
    return val_is_err(b) && key_eq(*val_ptr2err(a), *val_ptr2err(b));
  default:
    return val_eq(a, b);
  }
}

static bool is_unset(Val v) {
  return val_biteq(v, EMPTY_ITEM) || val_biteq(v, TOMBSTONE_ITEM);
}

#ifdef CLOX_DEBUG
static uint64_t finds = 0;
static uint64_t collisions = 0;

void collision_summary(void) {
  fprintf(stderr, "finds: %" PRIu64 ", collisions: %" PRIu64 ", ratio: %f\n",
          finds, collisions, (double)finds / (double)collisions);
}

void table_summary(const Table *t) {
  size_t cap = arraylist_cap(Entry)(&t->al);
  for (size_t i = 0; i < cap; i++) {
    Entry *entry = arraylist_get(Entry)(&t->al, i);
    if (val_biteq(entry->key, TOMBSTONE_ITEM)) {
      printf(".");
    } else if (val_biteq(entry->key, EMPTY_ITEM)) {
      printf(" ");
    } else {
      printf("#");
    }
  }
  printf("\n");
}

void collision_reset(void) {
  finds = 0;
  collisions = 0;
}
#endif

static Entry *table_find_entry(const Table *t, Val key) {
#ifdef CLOX_DEBUG
  finds++;
#endif
  size_t hash = val_hash(key);
  size_t cap = arraylist_cap(Entry)(&t->al);
  assert((cap & (cap - 1)) == 0); // only powers of 2
  size_t mask = cap - 1;
  size_t idx = hash & mask;
  Entry *first_tombstone = 0;
  for (;;) {
    Entry *entry = arraylist_get(Entry)(&t->al, idx);
    if (val_biteq(entry->key, EMPTY_ITEM)) {
      return first_tombstone != 0 ? first_tombstone : entry;
    } else if (val_biteq(entry->key, TOMBSTONE_ITEM)) {
      first_tombstone = first_tombstone == 0 ? entry : first_tombstone;
      goto skip;
    } else {
      if (key_eq(entry->key, key)) {
        return entry;
#ifdef CLOX_DEBUG
      } else {
        collisions++;
#endif
      }
    }
  skip:
    idx = (idx + 1) & mask; // idx = (idx + 1) % t->cap;
  }
}

static void table_grow(Table *t) {
  size_t old_cap = arraylist_cap(Entry)(&t->al);
  arraylist_grow(Entry)(&t->al);
  size_t new_cap = arraylist_cap(Entry)(&t->al);
  assert(new_cap > 0);
  for (size_t i = old_cap; i < new_cap; i++) {
    arraylist_get(Entry)(&t->al, i)->key = EMPTY_ITEM;
  }
  if (old_cap == 0) {
    return;
  }
  // Inline rehashing: find the 1st unset and and start rehashing entries to its
  // right which should either move between the 1st unset and their position or
  // to the new zone. Continue rehashing entries to the right. Then, rehash the
  // items before the 1st unset.
  size_t start = 0;
  // find the start position
  for (start = 0; start < old_cap; start++) {
    if (val_biteq(arraylist_get(Entry)(&t->al, start)->key, EMPTY_ITEM)) {
      break;
    }
  }
  assert(start < old_cap || old_cap == 0);
  // reset tombstones
  for (size_t i = 0; arraylist_len(Entry)(&t->al) > t->real_len; i++) {
    Entry *entry = arraylist_get(Entry)(&t->al, i);
    if (val_biteq(entry->key, TOMBSTONE_ITEM)) {
      entry->key = EMPTY_ITEM;
      arraylist_trunc(Entry)(&t->al, arraylist_len(Entry)(&t->al) - 1);
    }
  }
  assert(arraylist_len(Entry)(&t->al) == t->real_len);
  // rehash
  size_t remaining = t->real_len;
  assert((old_cap & (old_cap - 1)) == 0); // only powers of 2
  size_t mask = old_cap - 1;
  for (size_t i = (start + 1) & mask; remaining; i = (i + 1) & mask) {
    Entry *entry = arraylist_get(Entry)(&t->al, i);
    if (val_biteq(entry->key, EMPTY_ITEM)) {
      continue;
    }
    remaining--;
    Entry copy = *entry;
    entry->key = EMPTY_ITEM;
    *table_find_entry(t, copy.key) = copy;
  }
}

static bool not_reserved(Val v) {
  return !val_biteq(v, EMPTY_ITEM) && !val_biteq(v, TOMBSTONE_ITEM);
}

bool table_set(Table *t, Val key, Val val) {
  assert(not_reserved(key));
  assert(not_reserved(val));
  size_t len = arraylist_len(Entry)(&t->al);
  size_t cap = arraylist_cap(Entry)(&t->al);
  assert(len == 0 || len < cap);
  assert(len >= t->real_len);
  if (len + 1 > (cap / 7) * 5) {
    table_grow(t);
    len = arraylist_len(Entry)(&t->al); // len changes when tombsones clear
  }
  Entry *entry = table_find_entry(t, key);
  bool is_new = false;
  if (val_biteq(entry->key, TOMBSTONE_ITEM)) {
    is_new = true;
    entry->key = key;
  } else if (val_biteq(entry->key, EMPTY_ITEM)) {
    is_new = true;
    entry->key = key;
    arraylist_trunc(Entry)(&t->al, len + 1);
  } else {
    val_free(entry->val);
    val_free(key);
  }
  if (is_new) {
    t->real_len++;
  }
  entry->val = val;
  return is_new;
}

Val table_get(const Table *t, Val key, Val def) {
  assert(not_reserved(key));
  if (t->real_len == 0) {
    return def;
  }
  Entry *entry = table_find_entry(t, key);
  if (is_unset(entry->key)) {
    return def;
  }
  return entry->val;
}

bool table_del(Table *t, Val key) {
  assert(not_reserved(key));
  if (t->real_len == 0) {
    return false;
  }
  Entry *entry = table_find_entry(t, key);
  if (is_unset(entry->key)) {
    return false;
  }
  val_free(entry->key);
  val_free(entry->val);
  entry->key = TOMBSTONE_ITEM;
  t->real_len--;
  return true;
}

void table_free(Table *t) {
  assert(t->real_len < arraylist_cap(Entry)(&t->al) || t->real_len == 0);
  size_t remaining = t->real_len;
  for (size_t i = 0; remaining; i++) {
    Entry *entry = arraylist_get(Entry)(&t->al, i);
    if (is_unset(entry->key)) {
      continue;
    }
    remaining--;
    val_free(entry->key);
    val_free(entry->val);
  }
  arraylist_free(Entry)(&t->al);
  t->real_len = 0;
}

void table_print(const Table *t) {
  assert(t->real_len < arraylist_cap(Entry)(&t->al) || t->real_len == 0);
  printf("{");
  size_t remaining = t->real_len;
  for (size_t i = 0; remaining; i++) {
    Entry *entry = arraylist_get(Entry)(&t->al, i);
    if (is_unset(entry->key)) {
      continue;
    }
    remaining--;
    val_print(entry->key);
    printf(": ");
    val_print(entry->val);
    if (remaining > 1) {
      printf(", ");
    }
  }
  printf("}");
}

bool table_eq(const Table *a, const Table *b) { return true; }

extern inline void string_print(const String *);
extern inline size_t string_hash(String *);

extern inline void slice_print(const Slice *);
extern inline Slice slice(const char *, size_t len);
extern inline size_t slice_hash(Slice *);

extern inline Val list_get(const List *, size_t idx, Val def);
extern inline void list_set(List *l, size_t idx, Val val);
extern inline Val list_pop(List *, Val def);
extern inline void list_append(List *, Val);
extern inline size_t list_len(const List *);

extern inline size_t table_len(const Table *);
