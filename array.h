#ifndef clox_array_h
#define clox_array_h

#include <stddef.h> // size_t

typedef struct {
  size_t len;
  size_t cap;
  void *items;
} GenericArrayList;

void gal_reserve(GenericArrayList *, size_t item_size, size_t cap);
void gal_grow(GenericArrayList *, size_t item_size);
void gal_free(GenericArrayList *, size_t item_size);
void gal_seal(GenericArrayList *, size_t item_size);

#define ArrayList(T) ArrayList_##T

#define arraylist_reserve(T) arraylist_reserve_##T
#define arraylist_grow(T) arraylist_grow_##T
#define arraylist_len(T) arraylist_len_##T
#define arraylist_cap(T) arraylist_cap_##T
#define arraylist_get(T) arraylist_get_##T
#define arraylist_trunc(T) arraylist_trunc_##T
#define arraylist_append(T) arraylist_append_##T
#define arraylist_free(T) arraylist_free_##T
#define arraylist_seal(T) arraylist_seal_##T

#define arraylist_declare(T)                                                   \
  struct ArrayList(T) {                                                        \
    GenericArrayList gal;                                                      \
  };                                                                           \
                                                                               \
  inline void arraylist_reserve(T)(struct ArrayList(T) * l, size_t cap) {      \
    gal_reserve(&l->gal, sizeof(T), cap);                                      \
  }                                                                            \
                                                                               \
  inline void arraylist_grow(T)(struct ArrayList(T) * l) {                     \
    gal_grow(&l->gal, sizeof(T));                                              \
  }                                                                            \
                                                                               \
  inline size_t arraylist_len(T)(const struct ArrayList(T) * l) {              \
    return l->gal.len;                                                         \
  }                                                                            \
                                                                               \
  inline size_t arraylist_cap(T)(const struct ArrayList(T) * l) {              \
    return l->gal.cap;                                                         \
  }                                                                            \
                                                                               \
  inline T *arraylist_get(T)(const struct ArrayList(T) * l, size_t idx) {      \
    assert(idx < arraylist_cap(T)(l));                                         \
    return &((T *)(l->gal.items))[idx];                                        \
  }                                                                            \
                                                                               \
  inline void arraylist_trunc(T)(struct ArrayList(T) * l, size_t len) {        \
    assert(len <= arraylist_cap(T)(l));                                        \
    l->gal.len = len;                                                          \
  }                                                                            \
                                                                               \
  inline void arraylist_append(T)(struct ArrayList(T) * l, const T *i) {       \
    assert(arraylist_cap(T)(l) >= arraylist_len(T)(l));                        \
    if (arraylist_cap(T)(l) == arraylist_len(T)(l)) {                          \
      arraylist_grow(T)(l);                                                    \
    }                                                                          \
    assert(arraylist_cap(T)(l) > arraylist_len(T)(l));                         \
    *arraylist_get(T)(l, arraylist_len(T)(l)) = *i;                            \
    arraylist_trunc(T)(l, arraylist_len(T)(l) + 1);                            \
  }                                                                            \
                                                                               \
  inline void arraylist_free(T)(struct ArrayList(T) * l) {                     \
    gal_free(&l->gal, sizeof(T));                                              \
    *l = (struct ArrayList(T)){0};                                             \
  }                                                                            \
                                                                               \
  inline void arraylist_seal(T)(struct ArrayList(T) * l) {                     \
    gal_seal(&l->gal, sizeof(T));                                              \
  }                                                                            \
                                                                               \
  typedef struct ArrayList(T) ArrayList(T)

#define arraylist_define(T)                                                    \
  extern inline void arraylist_reserve(T)(ArrayList(T) *, size_t);             \
  extern inline void arraylist_grow(T)(ArrayList(T) *);                        \
  extern inline size_t arraylist_len(T)(const ArrayList(T) *);                 \
  extern inline size_t arraylist_cap(T)(const ArrayList(T) *);                 \
  extern inline T *arraylist_get(T)(const ArrayList(T) *, size_t);             \
  extern inline void arraylist_trunc(T)(ArrayList(T) *, size_t);               \
  extern inline void arraylist_append(T)(ArrayList(T) *, const T *);           \
  extern inline void arraylist_free(T)(ArrayList(T) *);                        \
  extern inline void arraylist_seal(T)(ArrayList(T) *)

#endif
