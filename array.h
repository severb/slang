#ifndef clox_array_h
#define clox_array_h

#include <stddef.h> // size_t

typedef struct {
  size_t len;
  size_t cap;
  void *items;
} AL;

void al_free(AL *a, size_t item_size);
void al_grow(AL *a, size_t item_size);
void al_seal(AL *a, size_t item_size);

#define ArrayList(T) ArrayList_##T

#define arraylist_grow(T) arraylist_grow_##T
#define arraylist_append(T) arraylist_append_##T
#define arraylist_get(T) arraylist_get_##T
#define arraylist_set(T) arraylist_set_##T
#define arraylist_free(T) arraylist_free_##T
#define arraylist_len(T) arraylist_len_##T
#define arraylist_cap(T) arraylist_cap_##T
#define arraylist_trunc(T) arraylist_trunc_##T
#define arraylist_seal(T) arraylist_seal_##T

#define arraylist_declare(T)                                                   \
  struct ArrayList(T) {                                                        \
    AL a;                                                                      \
  };                                                                           \
                                                                               \
  inline void arraylist_grow(T)(struct ArrayList(T) * l) {                     \
    al_grow(&l->a, sizeof(T));                                                 \
  }                                                                            \
                                                                               \
  inline void arraylist_append(T)(struct ArrayList(T) * l, T i) {              \
    AL *a = &l->a;                                                             \
    assert(a->cap >= a->len);                                                  \
    if (a->cap == a->len) {                                                    \
      arraylist_grow(T)(l);                                                    \
    }                                                                          \
    assert(a->cap > a->len);                                                   \
    ((T *)(a->items))[a->len++] = i;                                           \
  }                                                                            \
                                                                               \
  inline T arraylist_get(T)(const struct ArrayList(T) * l, size_t idx) {       \
    const AL *a = &l->a;                                                       \
    assert(idx < a->len);                                                      \
    return ((T *)(a->items))[idx];                                             \
  }                                                                            \
                                                                               \
  inline void arraylist_set(T)(struct ArrayList(T) * l, size_t idx, T i) {     \
    AL *a = &l->a;                                                             \
    assert(idx < a->len);                                                      \
    ((T *)(a->items))[idx] = i;                                                \
  }                                                                            \
                                                                               \
  inline void arraylist_free(T)(struct ArrayList(T) * l) {                     \
    AL *a = &l->a;                                                             \
    al_free(a, sizeof(T));                                                     \
    *a = (AL){0};                                                              \
  }                                                                            \
                                                                               \
  inline size_t arraylist_len(T)(const struct ArrayList(T) * l) {              \
    return l->a.len;                                                           \
  }                                                                            \
                                                                               \
  inline size_t arraylist_cap(T)(const struct ArrayList(T) * l) {              \
    return l->a.cap;                                                           \
  }                                                                            \
                                                                               \
  inline void arraylist_trunc(T)(struct ArrayList(T) * l, size_t len) {        \
    l->a.len = len;                                                            \
  }                                                                            \
                                                                               \
  inline void arraylist_seal(T)(struct ArrayList(T) * l) {                     \
    AL *a = &l->a;                                                             \
    al_seal(a, sizeof(T));                                                     \
  }                                                                            \
                                                                               \
  typedef struct ArrayList(T) ArrayList(T)

#define arraylist_define(T)                                                    \
  extern inline void arraylist_append(T)(ArrayList(T) *, T);                   \
  extern inline T arraylist_get(T)(const ArrayList(T) *, size_t);              \
  extern inline void arraylist_set(T)(ArrayList(T) *, size_t, T);              \
  extern inline void arraylist_free(T)(ArrayList(T) *);                        \
  extern inline size_t arraylist_len(T)(const ArrayList(T) *);                 \
  extern inline size_t arraylist_cap(T)(const ArrayList(T) *);                 \
  extern inline void arraylist_trunc(T)(ArrayList(T) *, size_t);               \
  extern inline void arraylist_seal(T)(ArrayList(T) *)

#endif
