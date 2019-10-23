#ifndef clox_mem_h
#define clox_mem_h

#include <stddef.h>

#define ALLOCATE(type) (type *)reallocate(0, 0, sizeof(type))
#define FREE(pointer, type) reallocate(pointer, sizeof(type), 0)

#define GROW_ARRAY(pointer, type, old_count, new_count)                        \
  ((((old_count) < SIZE_MAX / sizeof(type)) &&                                 \
    ((new_count) < SIZE_MAX / sizeof(type)))                                   \
       ? reallocate((pointer), sizeof(type) * (old_count),                     \
                    sizeof(type) * (new_count))                                \
       : 0)
#define FREE_ARRAY(pointer, type, old_count)                                   \
  do {                                                                         \
    if ((old_count) < SIZE_MAX / sizeof(type))                                 \
      reallocate((pointer), sizeof(type) * (old_count), 0);                    \
    else                                                                       \
      assert(0);                                                               \
  } while (0);

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
  } while (0);

void *reallocate(void *previous, size_t oldSize, size_t newSize);

#endif
