#ifndef clox_listgen_h
#define clox_listgen_h

#include "mem.h" // FREE_ARRAY

#include <assert.h> // assert
#include <stdio.h>  // fptus, stderr
#include <stdlib.h> // abort, size_t

#define LIST_DECL(name, type)                                                  \
                                                                               \
  typedef struct List_##name {                                                 \
    size_t cap;                                                                \
    size_t len;                                                                \
    type *vals;                                                                \
  } List_##name;                                                               \
                                                                               \
  List_##name *list_##name##_init(List_##name *);                              \
  void list_##name##_destroy(List_##name *);                                   \
                                                                               \
  void list_##name##_grow(List_##name *);                                      \
  void list_##name##_seal(List_##name *);                                      \
                                                                               \
  static inline size_t list_##name##_append(List_##name *list, type val) {     \
    assert(list->cap >= list->len);                                            \
    if (list->cap == list->len) {                                              \
      list_##name##_grow(list);                                                \
    }                                                                          \
    size_t idx = list->len;                                                    \
    list->len++;                                                               \
    list->vals[idx] = val;                                                     \
    return idx;                                                                \
  }                                                                            \
                                                                               \
  static inline type list_##name##_get(const List_##name *list, size_t idx) {  \
    assert(idx < list->len);                                                   \
    return list->vals[idx];                                                    \
  }                                                                            \
                                                                               \
  static inline type list_##name##_pop(List_##name *list) {                    \
    assert(list->len);                                                         \
    list->len--;                                                               \
    return list->vals[list->len];                                              \
  }

#define LIST_IMPL(name, type)                                                  \
                                                                               \
  List_##name *list_##name##_init(List_##name *list) {                         \
    if (list != 0) {                                                           \
      *list = (List_##name){0};                                                \
    }                                                                          \
    return list;                                                               \
  }                                                                            \
                                                                               \
  void list_##name##_destroy(List_##name *list) {                              \
    if (list == 0) {                                                           \
      return;                                                                  \
    }                                                                          \
    EXTRA_DESTROY                                                              \
    FREE_ARRAY(list->vals, type, list->cap);                                   \
    list_##name##_init(list);                                                  \
  }                                                                            \
                                                                               \
  void list_##name##_grow(List_##name *list) {                                 \
    assert(list->cap >= list->len);                                            \
    size_t old_cap = list->cap;                                                \
    if (list->cap < 8) {                                                       \
      list->cap = 8;                                                           \
    } else if (list->cap <= (SIZE_MAX / 2 / sizeof(type))) {                   \
      list->cap *= 2;                                                          \
    } else {                                                                   \
      fputs("list exceeds its maximum capacity", stderr);                      \
      abort();                                                                 \
    }                                                                          \
    list->vals = GROW_ARRAY(list->vals, type, old_cap, list->cap);             \
    if (list->vals == 0) {                                                     \
      fputs("not enough memory to grow the list", stderr);                     \
      abort();                                                                 \
    }                                                                          \
  }                                                                            \
                                                                               \
  void list_##name##_seal(List_##name *list) {                                 \
    assert(list->cap >= list->len);                                            \
    list->vals = GROW_ARRAY(list->vals, type, list->cap, list->len);           \
    list->cap = list->len;                                                     \
  }

#endif
