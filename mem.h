#ifndef clox_mem_h
#define clox_mem_h

#include <assert.h> // assert
#include <stddef.h> // size_t
#include <stdint.h> // SIZE_MAX

void *mem_reallocate(void *, size_t old_size, size_t new_size);
inline void *mem_allocate(size_t item_size) {
  return mem_reallocate(0, 0, item_size);
}
inline void mem_free(void *p, size_t item_size) {
  mem_reallocate(p, item_size, 0);
}

inline void *mem_resize_array(void *p, size_t item_size, size_t old_len,
                              size_t new_len) {
  assert(old_len < SIZE_MAX / item_size);
  return (new_len < SIZE_MAX / item_size)
             ? mem_reallocate(p, item_size * old_len, item_size * new_len)
             : 0;
}

inline void mem_free_array(void *p, size_t item_size, size_t old_len) {
  assert(old_len < SIZE_MAX / item_size);
  mem_reallocate(p, item_size * old_len, 0);
}

inline void *mem_allocate_flex(size_t type_size, size_t item_size, size_t len) {
  return (len < (SIZE_MAX - type_size) / item_size)
             ? mem_reallocate(0, 0, type_size + item_size * len)
             : 0;
}

inline void mem_free_flex(void *p, size_t type_size, size_t item_size,
                          size_t len) {
  assert(len < (SIZE_MAX - type_size) / item_size);
  mem_reallocate(p, type_size + item_size * len, 0);
}

void mem_allocation_summary(void);

#endif
