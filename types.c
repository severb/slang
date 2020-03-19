#include "types.h"

#include "mem.h" // mem_allocate_flex, mem_free_array, mem_resize_array
#include "val.h" // val_free, val_print

#include <stdio.h>  // printf
#include <stdlib.h> // abort
#include <string.h> // memcpy, size_t

String *string_new_empty(size_t len) {
  String *res = mem_allocate_flex(sizeof(String), sizeof(char), len);
  if (res == 0) {
    return 0;
  }
  res->hash = 0;
  res->len = 0;
  return res;
}

String *string_new(char const *c, size_t len) {
  String *res = string_new_empty(len);
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
    res = UINT64_C(7777777);
  }
  return (size_t)res;
}

void list_free(List *l) {
  for (size_t i = 0; i < l->len; i++) {
    val_free(l->items[i]);
  }
  mem_free_array(l->items, sizeof(l->items[0]), l->cap);
  *l = (List){0};
}

void list_print(const List *l) {
  printf("[");
  for (size_t i = 0; i < l->len; i++) {
    val_print(l->items[i]);
    if (i + 1 < l->len) {
      printf(", ");
    }
  }
  printf("]");
}

static void list_grow(List *l, size_t item_size) {
  size_t old_cap = l->cap;
  if (l->cap < 8) {
    l->cap = 8;
  } else if (l->cap < (SIZE_MAX / 2 / item_size)) {
    l->cap *= 2;
  } else {
    goto out_of_memory;
  }
  l->items = mem_resize_array(l->items, sizeof(l->items[0]), old_cap, l->cap);
  if (l->items == 0) {
    goto out_of_memory;
  }
  return;
out_of_memory:
  fputs("out of memory", stderr);
  abort();
}

void list_append(List *l, Val v) {
  assert(l->cap >= l->len);
  if (l->cap == l->len) {
    list_grow(l, sizeof(Val));
  }
  assert(l->cap > l->len);
  l->items[l->len] = v;
  l->len++;
}

bool list_eq(const List *a, const List *b) { return a->len == b->len; }

static const Val empty_item = USR_SYMBOL(0);
static const Val tombstone_item = USR_SYMBOL(1);

static bool key_eq(Val a, Val b) {
  switch (val_type(a)) {
  case VAL_TABLE:
  case VAL_LIST:
    // compare lists and tables by identity, not element by element
    return val_ptr2ptr(a) == val_ptr2ptr(b);
  case VAL_ERR:
    // avoid comparing lists and tables element by element when inside errors
    return val_is_err(b) && key_eq(*val_ptr2err(a), *val_ptr2err(b));
  default:
    return val_eq(a, b);
  }
}

static bool is_unset(Val v) {
  return val_biteq(v, empty_item) || val_biteq(v, tombstone_item);
}

static Entry *table_find_item(const Table *t, Val key) {
  size_t hash = val_hash(key);
  size_t idx = hash % t->cap;
  Entry *first_tombstone = 0;
  for (;;) {
    Entry *entry = &t->items[idx];
    if (val_biteq(entry->key, empty_item)) {
      return first_tombstone != 0 ? first_tombstone : entry;
    } else if (val_biteq(entry->key, tombstone_item)) {
      first_tombstone = first_tombstone == 0 ? entry : first_tombstone;
      goto skip;
    } else {
      if (key_eq(entry->key, key)) {
        return entry;
      }
    }
  skip:
    idx = (idx + 1) % t->cap;
  }
}

void table_free(Table *t) {
  size_t seen = 0;
  for (size_t i = 0; i < t->cap && seen < t->len; i++) {
    Val key = t->items[i].key;
    if (is_unset(key)) {
      continue;
    }
    seen++;
    val_free(key);
    val_free(t->items[i].val);
  }
  assert(seen == t->len);
  mem_free_array(t->items, sizeof(t->items[0]), t->cap);
  *t = (Table){0};
}

void table_print(const Table *t) {
  printf("{");
  size_t seen = 0;
  for (size_t i = 0; i < t->cap && seen < t->len; i++) {
    Val key = t->items[i].key;
    if (is_unset(key)) {
      continue;
    }
    seen++;
    val_print(key);
    printf(": ");
    val_print(t->items[i].val);
    if (seen < t->len) {
      printf(", ");
    }
  }
  assert(seen = t->len);
  printf("}");
}

bool table_eq(const Table *a, const Table *b) { return a->len == b->len; }

extern inline void string_print(const String *);
extern inline size_t string_hash(String *);
extern inline void slice_print(const Slice *);
extern inline Slice slice(const char *, size_t len);
extern inline size_t slice_hash(Slice *);
extern inline Val list_get(const List *, size_t idx, Val def);
extern inline Val list_pop(List *, Val def);
