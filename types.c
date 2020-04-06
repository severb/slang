#include "types.h"

#include "mem.h" // mem_allocate_flex, mem_free_array, mem_resize_array
#include "val.h" // val_free, val_print

#include <stdint.h> // uint64_t, UINT64_C
#include <stdio.h>  // printf
#include <stdlib.h> // abort
#include <string.h> // memcpy, size_t

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

struct Seq {
  size_t cap;
  size_t len;
  void *items;
};

static void seq_grow(struct Seq *s, size_t item_size) {
  size_t old_cap = s->cap;
  if (s->cap < 8) {
    s->cap = 8;
  } else if (s->cap < (SIZE_MAX / 2 / item_size)) {
    s->cap *= 2;
  } else {
    goto out_of_memory;
  }
  s->items = mem_resize_array(s->items, item_size, old_cap, s->cap);
  if (s->items == 0) {
    goto out_of_memory;
  }
  return;
out_of_memory:
  fputs("out of memory when growing sequence", stderr);
  abort();
}

void list_append(List *l, Val v) {
  assert(l->cap >= l->len);
  if (l->cap == l->len) {
    seq_grow((struct Seq *)l, sizeof(l->items[0]));
  }
  assert(l->cap > l->len);
  l->items[l->len] = v;
  l->len++;
}

bool list_eq(const List *a, const List *b) { return a->len == b->len; }

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
  fprintf(stderr, "finds: %lu, collisions: %lu, ratio: %f\n", finds, collisions,
          (double)finds / (double)collisions);
}

void table_summary(const Table *t) {
  for (size_t i = 0; i < t->cap; i++) {
    if (val_biteq(t->items[i].key, TOMBSTONE_ITEM)) {
      printf(".");
    } else if (val_biteq(t->items[i].key, EMPTY_ITEM)) {
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
  assert((t->cap & (t->cap - 1)) == 0); // only powers of 2
  size_t mask = t->cap - 1;
  size_t idx = hash & mask;
  Entry *first_tombstone = 0;
  for (;;) {
    Entry *entry = &t->items[idx];
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
  size_t old_cap = t->cap;
  seq_grow((struct Seq *)t, sizeof(t->items[0]));
  assert(t->cap > 0);
  for (size_t i = old_cap; i < t->cap; i++) {
    t->items[i].key = EMPTY_ITEM;
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
    if (val_biteq(t->items[start].key, EMPTY_ITEM)) {
      break;
    }
  }
  assert(start < old_cap || old_cap == 0);
  // reset tombstones
  for (size_t i = 0; t->len > t->real_len; i++) {
    if (val_biteq(t->items[i].key, TOMBSTONE_ITEM)) {
      t->items[i].key = EMPTY_ITEM;
      t->len--;
    }
  }
  assert(t->len == t->real_len);
  // rehash
  size_t remaining = t->real_len;
  for (size_t i = (start + 1) % old_cap; remaining; i = (i + 1) % old_cap) {
    Entry *e = &t->items[i];
    if (val_biteq(e->key, EMPTY_ITEM)) {
      continue;
    }
    remaining--;
    Entry copy = *e;
    e->key = EMPTY_ITEM;
    *table_find_entry(t, copy.key) = copy;
  }
}

static bool not_reserved(Val v) {
  return !val_biteq(v, EMPTY_ITEM) && !val_biteq(v, TOMBSTONE_ITEM);
}

bool table_set(Table *t, Val key, Val val) {
  assert(not_reserved(key));
  assert(not_reserved(val));
  assert(t->len == 0 || t->len < t->cap);
  assert(t->len >= t->real_len);
  if (t->len + 1 > (t->cap / 7) * 5) {
    table_grow(t);
  }
  Entry *entry = table_find_entry(t, key);
  bool is_new = false;
  if (val_biteq(entry->key, TOMBSTONE_ITEM)) {
    is_new = true;
    entry->key = key;
  } else if (val_biteq(entry->key, EMPTY_ITEM)) {
    is_new = true;
    entry->key = key;
    t->len++;
  } else {
    val_free(entry->val);
    val_free(key);
  }
  t->real_len += is_new;
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
  assert(t->real_len < t->cap || t->real_len == 0);
  size_t remaining = t->real_len;
  for (size_t i = 0; remaining; i++) {
    Val key = t->items[i].key;
    if (is_unset(key)) {
      continue;
    }
    remaining--;
    val_free(key);
    val_free(t->items[i].val);
  }
  mem_free_array(t->items, sizeof(t->items[0]), t->cap);
  *t = (Table){0};
}

void table_print(const Table *t) {
  assert(t->real_len < t->cap || t->real_len == 0);
  printf("{");
  size_t remaining = t->real_len;
  for (size_t i = 0; remaining; i++) {
    Val key = t->items[i].key;
    if (is_unset(key)) {
      continue;
    }
    remaining--;
    val_print(key);
    printf(": ");
    val_print(t->items[i].val);
    if (remaining > 1) {
      printf(", ");
    }
  }
  printf("}");
}

bool table_eq(const Table *a, const Table *b) { return a->len == b->len; }

extern inline void string_print(const String *);
extern inline size_t string_hash(String *);
extern inline void slice_print(const Slice *);
extern inline Slice slice(const char *, size_t len);
extern inline size_t slice_hash(Slice *);
extern inline Val list_get(const List *, size_t idx, Val def);
extern inline void list_set(List *l, size_t idx, Val val);
extern inline Val list_pop(List *, Val def);
