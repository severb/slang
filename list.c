#include "list.h"

#include "mem.h" // FREE_ARRAY
#include "val.h" // val_destroy

#include <assert.h> // assert
#include <stdio.h>  // fptus, stderr
#include <stdlib.h> // abort, size_t

List *list_init(List *list) {
  if (list == 0)
    return 0;
  *list = (List){.cap = 0, .len = 0, .vals = 0};
  return list;
}

void list_destroy(List *list) {
  if (list == 0)
    return;
  for (size_t i = 0; i < list->len; i++) {
    val_destroy(list->vals[i]);
  }
  FREE_ARRAY(list->vals, Val, list->cap);
  list_init(list);
}

void list_grow(List *list) {
  assert(list->cap >= list->len);
  size_t old_cap = list->cap;
  if (list->cap < 8) {
    list->cap = 8;
  } else if (list->cap <= (SIZE_MAX / 2 / sizeof(Val))) {
    list->cap *= 2;
  } else {
    fputs("list exceeds its maximum capacity", stderr);
    abort();
  }
  list->vals = GROW_ARRAY(list->vals, Val, old_cap, list->cap);
  if (list->vals == 0) {
    fputs("not enough memory to grow the list", stderr);
    abort();
  }
}

void list_seal(List *list) {
  assert(list->cap >= list->len);
  list->vals = GROW_ARRAY(list->vals, Val, list->cap, list->len);
  list->cap = list->len;
}
