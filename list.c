#include "list.h"

#include "mem.h"
#include "str.h"
#include "val.h"

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
    val_destroy(&list->vals[i]);
  }
  FREE_ARRAY(list->vals, Val, list->cap);
  list_init(list);
}

// Append the value and return its index. SIZE_MAX is an invalid index and it's
// returend on error.
size_t list_append(List *list, Val val) {
  assert(list->cap >= list->len);
  if (list->cap == list->len) {
    size_t old_cap = list->cap;
    if (list->cap < 8) {
      list->cap = 8;
    } else if (list->cap <= (SIZE_MAX / 2 / sizeof(Val) - 1)) {
      list->cap *= 2;
    } else {
      return SIZE_MAX;
    }
    list->vals = GROW_ARRAY(list->vals, Val, old_cap, list->cap);
    if (list->vals == 0) {
      return SIZE_MAX;
    }
  }
  size_t idx = list->len;
  list->len++;
  list->vals[idx] = val;
  return idx;
}

void list_seal(List *list) {
  assert(list->cap >= list->len);
  list->vals = GROW_ARRAY(list->vals, Val, list->cap, list->len);
  list->cap = list->len;
}

Val list_get(const List *list, size_t i) {
  assert(list->cap >= list->len);
  if (i < list->len)
    return list->vals[i]; // TODO: return reference
  return 0;               // TODO: return error
}

Val list_pop(List *list) {
  if (list->len == 0)
    return 0; // TODO: return error
  list->len--;
  return list->vals[list->len]; // TODO: return reference
}
