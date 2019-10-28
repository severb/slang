#ifndef clox_list_h
#define clox_list_h

#include "val.h"

typedef struct List {
  size_t cap;
  size_t len;
  Val *vals;
} List;

List *list_init(List *);
void list_destroy(List *);

size_t list_append(List *, Val);
void list_seal(List *);
Val list_get(const List *, size_t idx);
Val list_pop(List *);

#endif
