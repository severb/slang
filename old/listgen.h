#ifndef clox_listgen_h
#define clox_listgen_h

#include "mem.h" // FREE_ARRAY

#include <assert.h> // assert
#include <stdio.h>  // fputs, stderr
#include <stdlib.h> // abort, size_t

#define LIST_TYPE(listtype, valtype)                                           \
  typedef struct listtype {                                                    \
    size_t cap;                                                                \
    size_t len;                                                                \
    valtype *vals;                                                             \
  } listtype;

#define LIST_DECL(listtype, prefix, valtype, modifier)                         \
  modifier listtype *prefix##_init(listtype *);                                \
  modifier void prefix##_destroy(listtype *);                                  \
                                                                               \
  modifier void prefix##_grow(listtype *);                                     \
  modifier void prefix##_seal(listtype *);                                     \
                                                                               \
  static inline size_t prefix##_append(listtype *list, valtype val) {          \
    assert(list->cap >= list->len);                                            \
    if (list->cap == list->len) {                                              \
      prefix##_grow(list);                                                     \
    }                                                                          \
    size_t idx = list->len;                                                    \
    list->len++;                                                               \
    list->vals[idx] = val;                                                     \
    return idx;                                                                \
  }                                                                            \
                                                                               \
  static inline valtype prefix##_get(const listtype *list, size_t idx) {       \
    assert(idx < list->len);                                                   \
    return list->vals[idx];                                                    \
  }                                                                            \
                                                                               \
  static inline valtype prefix##_pop(listtype *list) {                         \
    assert(list->len);                                                         \
    list->len--;                                                               \
    return list->vals[list->len];                                              \
  }

#define LIST_IMPL(listtype, prefix, valtype, modifier)                         \
  modifier listtype *prefix##_init(listtype *list) {                           \
    if (list != 0) {                                                           \
      *list = (listtype){0};                                                   \
    }                                                                          \
    return list;                                                               \
  }                                                                            \
                                                                               \
  modifier void prefix##_destroy(listtype *list) {                             \
    if (list == 0) {                                                           \
      return;                                                                  \
    }                                                                          \
    EXTRA_DESTROY                                                              \
    FREE_ARRAY(list->vals, valtype, list->cap);                                \
    prefix##_init(list);                                                       \
  }                                                                            \
                                                                               \
  modifier void prefix##_grow(listtype *list) {                                \
    assert(list->cap >= list->len);                                            \
    size_t old_cap = list->cap;                                                \
    if (list->cap < 8) {                                                       \
      list->cap = 8;                                                           \
    } else if (list->cap <= (SIZE_MAX / 2 / sizeof(valtype))) {                \
      list->cap *= 2;                                                          \
    } else {                                                                   \
      fputs("list exceeds its maximum capacity", stderr);                      \
      abort();                                                                 \
    }                                                                          \
    list->vals = GROW_ARRAY(list->vals, valtype, old_cap, list->cap);          \
    if (list->vals == 0) {                                                     \
      fputs("not enough memory to grow the list", stderr);                     \
      abort();                                                                 \
    }                                                                          \
  }                                                                            \
                                                                               \
  modifier void prefix##_seal(listtype *list) {                                \
    assert(list->cap >= list->len);                                            \
    list->vals = GROW_ARRAY(list->vals, valtype, list->cap, list->len);        \
    list->cap = list->len;                                                     \
  }

#endif
