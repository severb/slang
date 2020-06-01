#ifndef slang_dynarray_h
#define slang_dynarray_h

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *items;
    size_t len;
    size_t cap;
} DynamicArrayT;

void dynarray_reserve_T(DynamicArrayT *, size_t cap, size_t item_size);
void dynarray_grow_T(DynamicArrayT *, size_t item_size);
void dynarray_seal_T(DynamicArrayT *, size_t item_size);
void dynarray_destroy_T(DynamicArrayT *, size_t item_size);
void dynarray_free_T(DynamicArrayT *, size_t item_size);

#define DynamicArray(T) DynamicArray_GEN_##T

#define dynarray_reserve(T) dynarray_reserve_GEN_##T
#define dynarray_grow(T) dynarray_grow_GEN_##T
#define dynarray_len(T) dynarray_len_GEN_##T
#define dynarray_cap(T) dynarray_cap_GEN_##T
#define dynarray_get(T) dynarray_get_GEN_##T
#define dynarray_trunc(T) dynarray_trunc_GEN_##T
#define dynarray_append(T) dynarray_append_GEN_##T
#define dynarray_destroy(T) dynarray_free_GEN_##T
#define dynarray_free(T) denarray_free_GEN_##T
#define dynarray_seal(T) dynarray_seal_GEN_##T

#define dynarray_declare(T)                                                                        \
    struct DynamicArray(T) {                                                                       \
        DynamicArrayT array;                                                                       \
    };                                                                                             \
                                                                                                   \
    inline void dynarray_reserve(T)(struct DynamicArray(T) * l, size_t cap) {                      \
        dynarray_reserve_T(&l->array, cap, sizeof(T));                                             \
    }                                                                                              \
                                                                                                   \
    inline void dynarray_grow(T)(struct DynamicArray(T) * l) {                                     \
        dynarray_grow_T(&l->array, sizeof(T));                                                     \
    }                                                                                              \
                                                                                                   \
    inline size_t dynarray_len(T)(const struct DynamicArray(T) * l) { return l->array.len; }       \
                                                                                                   \
    inline size_t dynarray_cap(T)(const struct DynamicArray(T) * l) { return l->array.cap; }       \
                                                                                                   \
    inline T *dynarray_get(T)(const struct DynamicArray(T) * l, size_t idx) {                      \
        assert(idx < dynarray_cap(T)(l) && "dynarray get index out of range");                     \
        return &((T *)(l->array.items))[idx];                                                      \
    }                                                                                              \
                                                                                                   \
    inline void dynarray_trunc(T)(struct DynamicArray(T) * l, size_t len) {                        \
        assert(len <= dynarray_cap(T)(l) && "dynarray trunc index out of range");                  \
        l->array.len = len;                                                                        \
    }                                                                                              \
                                                                                                   \
    inline void dynarray_append(T)(struct DynamicArray(T) * l, const T *i) {                       \
        assert(dynarray_cap(T)(l) >= dynarray_len(T)(l) && "dynarray invariant");                  \
        if (dynarray_cap(T)(l) == dynarray_len(T)(l)) {                                            \
            dynarray_grow(T)(l);                                                                   \
        }                                                                                          \
        assert(dynarray_cap(T)(l) > dynarray_len(T)(l) && "dynarray invariant");                   \
        *dynarray_get(T)(l, dynarray_len(T)(l)) = *i;                                              \
        dynarray_trunc(T)(l, dynarray_len(T)(l) + 1);                                              \
    }                                                                                              \
                                                                                                   \
    inline void dynarray_destroy(T)(struct DynamicArray(T) * l) {                                  \
        dynarray_destroy_T(&l->array, sizeof(T));                                                  \
    }                                                                                              \
                                                                                                   \
    inline void dynarray_free(T)(struct DynamicArray(T) * l) {                                     \
        dynarray_free_T(&l->array, sizeof(T));                                                     \
    }                                                                                              \
                                                                                                   \
    inline void dynarray_seal(T)(struct DynamicArray(T) * l) {                                     \
        dynarray_seal_T(&l->array, sizeof(T));                                                     \
    }                                                                                              \
                                                                                                   \
    typedef struct DynamicArray(T) DynamicArray(T)

#define dynarray_define(T)                                                                         \
    extern inline void dynarray_reserve(T)(DynamicArray(T) *, size_t);                             \
    extern inline void dynarray_grow(T)(DynamicArray(T) *);                                        \
    extern inline size_t dynarray_len(T)(const DynamicArray(T) *);                                 \
    extern inline size_t dynarray_cap(T)(const DynamicArray(T) *);                                 \
    extern inline T *dynarray_get(T)(const DynamicArray(T) *, size_t);                             \
    extern inline void dynarray_trunc(T)(DynamicArray(T) *, size_t);                               \
    extern inline void dynarray_append(T)(DynamicArray(T) *, const T *);                           \
    extern inline void dynarray_destroy(T)(DynamicArray(T) *);                                     \
    extern inline void dynarray_free(T)(DynamicArray(T) *);                                        \
    extern inline void dynarray_seal(T)(DynamicArray(T) *)

// predefined dynamic lists
dynarray_declare(size_t);
dynarray_declare(uint8_t);

#endif
