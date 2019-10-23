#ifndef clox_mem_h
#define clox_mem_h

#include <stddef.h> // size_t
#include <stdint.h> // SIZE_MAX

#define ALLOCATE(type) (type *)reallocate(0, 0, sizeof(type))
#define FREE(pointer, type) reallocate(pointer, sizeof(type), 0)

#define GROW_ARRAY(pointer, type, old_len, new_len)                            \
  ((((old_len) < SIZE_MAX / sizeof(type)) &&                                   \
    ((new_len) < SIZE_MAX / sizeof(type)))                                     \
       ? reallocate((pointer), sizeof(type) * (old_len),                       \
                    sizeof(type) * (new_len))                                  \
       : 0)
#define FREE_ARRAY(pointer, type, old_len)                                     \
  do {                                                                         \
    if ((old_len) < SIZE_MAX / sizeof(type))                                   \
      reallocate((pointer), sizeof(type) * (old_len), 0);                      \
    else                                                                       \
      assert(0);                                                               \
  } while (0)

#define ALLOCATE_FLEX(type, array_type, len)                                   \
  ((len) < ((SIZE_MAX - sizeof(type)) / sizeof(array_type))                    \
       ? reallocate(0, 0, sizeof(type) + sizeof(array_type) * (len))           \
       : 0)
#define FREE_FLEX(pointer, type, array_type, len)                              \
  do {                                                                         \
    if ((len) < (SIZE_MAX - sizeof(type)) / sizeof(array_type))                \
      reallocate((pointer), sizeof(type) + sizeof(array_type) * (len), 0);     \
    else                                                                       \
      assert(0);                                                               \
  } while (0)

void *reallocate(void *, size_t old_size, size_t new_size);
extern size_t allocated_memory;

#endif
