#ifndef slang_mempool_h
#define slang_mempool_h

#include <stddef.h> // size_t
#include <stdlib.h> // malloc

#include "mem.h"

// A memory pool allocator
#define MemPool(T) MemPool_GEN_##T

#define mempool_new(T) pool_new_GEN_##T
#define mempool_del(T) pool_del_GEN_##T
#define mempool_alloc(T) pool_alloc_GEN_##T
#define mempool_free(T) pool_free_GEN_##T

#define mempool_declare(T)                                                     \
                                                                               \
  struct MemPool(T) {                                                          \
    size_t cap;                                                                \
    size_t next_empty;                                                         \
    union MemPool(T##_Cell) {                                                  \
      size_t next_empty;                                                       \
      Point2d cell;                                                            \
    }                                                                          \
    pool[];                                                                    \
  };                                                                           \
                                                                               \
  struct MemPool(T) * mempool_new(T)(size_t);                                  \
  void mempool_del(T)(struct MemPool(T) *);                                    \
                                                                               \
  inline T *mempool_alloc(T)(struct MemPool(T) * p) {                          \
    if (p->next_empty >= p->cap) {                                             \
      return malloc(sizeof(struct MemPool(T)));                                \
    }                                                                          \
    size_t next_empty = p->pool[p->next_empty].next_empty;                     \
    T *result = &p->pool[p->next_empty].cell;                                  \
    p->next_empty = next_empty;                                                \
    return result;                                                             \
  }                                                                            \
                                                                               \
  inline void mempool_free(T)(struct MemPool(T) * p, T * ptr) {                \
    union MemPool(T##_Cell) *uptr = (union MemPool(T##_Cell) *)ptr;            \
    if (&p->pool[0] <= uptr && uptr < &p->pool[p->cap]) {                      \
      ptrdiff_t idx = uptr - &p->pool[0];                                      \
      p->pool[idx].next_empty = p->next_empty;                                 \
      p->next_empty = idx;                                                     \
    } else {                                                                   \
      free(ptr);                                                               \
    }                                                                          \
  }                                                                            \
  typedef struct MemPool(T) MemPool(T)

#define mempool_define(T)                                                      \
                                                                               \
  MemPool(T) * mempool_new(T)(size_t size) {                                   \
    size_t struct_size = sizeof(MemPool(T));                                   \
    size_t cell_size = sizeof(union MemPool(T##_Cell));                        \
    MemPool(T) *p = mem_allocate_flex(struct_size, cell_size, size);           \
    if (!p) {                                                                  \
      return 0;                                                                \
    }                                                                          \
    for (size_t i = 0; i < size; i++) {                                        \
      p->pool[i].next_empty = i + 1;                                           \
    }                                                                          \
    return p;                                                                  \
  }                                                                            \
                                                                               \
  void mempool_del(T)(MemPool(T) * p) {                                        \
    size_t struct_size = sizeof(MemPool(T));                                   \
    size_t cell_size = sizeof(union MemPool(T##_Cell));                        \
    MemPool(T) *p = mem_free_flex(p, struct_size, cell_size, p->cap);          \
  }                                                                            \
                                                                               \
  extern inline T *mempool_alloc(T)(MemPool(T) *);                             \
  extern inline void mempool_free(T)(MemPool(T) *, T *)

#endif
