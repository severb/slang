#include "list.h"

#include "listgen.h" // LIST_C
#include "val.h"     // Val, val_destroy

#define EXTRA_DESTROY                                                          \
  for (size_t i = 0; i < list->len; i++) {                                     \
    val_destroy(list->vals[i]);                                                \
  }

LIST_C(val, Val)

#undef EXTRA_DESTROY
