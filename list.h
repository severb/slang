#ifndef clox_list_h
#define clox_list_h

#include "listgen.h" // LIST_H
#include "val.h"     // Val, ref

LIST_H(val, Val)

typedef List_val List;

#define list_init(l) list_val_init(l)
#define list_destroy(l) list_val_destroy(l)
#define list_grow(l) list_val_grow(l)
#define list_seal(l) list_val_seal(l)
#define list_append(l, v) list_val_append(l, v)
#define list_get(l, i) list_val_get(l, i)
#define list_pop(l) list_val_pop(l)

static inline Val list_getref(const List *list, size_t idx) {
  return ref(list_get(list, idx));
}

#endif
