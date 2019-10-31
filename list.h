#ifndef clox_list_h
#define clox_list_h

#include "val.h" // Val, ref

#include <assert.h> // assert
#include <stddef.h> // size_t

typedef struct List {
  size_t cap;
  size_t len;
  Val *vals;
} List;

List *list_init(List *);
void list_destroy(List *);

void list_grow(List *);
void list_seal(List *);

static inline size_t list_append(List *list, Val val) {
  assert(list->cap >= list->len);
  if (list->cap == list->len) {
    list_grow(list);
  }
  size_t idx = list->len;
  list->len++;
  list->vals[idx] = val;
  return idx;
}

#define list_get_(l, i) ((l).vals[(i)])
static inline Val list_get(const List *list, size_t idx) {
  assert(idx < list->len);
  return list_get_(*list, idx);
}

static inline Val list_getref(const List *list, size_t idx) {
  return ref(list_get(list, idx));
}

static inline Val list_pop(List *list) {
  assert(list->len);
  list->len--;
  return list->vals[list->len];
}

#endif
