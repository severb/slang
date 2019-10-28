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

// list_append() appends the value and return its index. SIZE_MAX is an invalid
// index and it's returend on error.
size_t list_append(List *, Val);

// list_seal() frees the unused slots eagarly allocated to the list. The next
// append causes the list to grow.
void list_seal(List *);

// list_get() gets an element from position idx from the list. If the idx is
// too large, it returns an error Val.
Val list_get(const List *, size_t idx);

// list_pop() pops the last element from the list. If the list is empty, it
// returns an error Val.
Val list_pop(List *);

#endif
