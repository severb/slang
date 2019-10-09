#ifndef clox_mem_h
#define clox_mem_h

#include <stddef.h>

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

#define GROW_ARRAY(pointer, type, old_count, new_count)                        \
  reallocate(pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define FREE_ARRAY(pointer, type, old_count)                                   \
  reallocate(pointer, sizeof(type) * (old_count), 0)

#define ALLOCATE(type) (type *)reallocate(NULL, 0, sizeof(type))

#define FREE(pointer, type) reallocate(pointer, sizeof(type), 0)

#define ALLOCATE_FLEX(type, array_type, length)                                \
  reallocate(NULL, 0, sizeof(type) + sizeof(array_type) * length)

#define FREE_FLEX(pointer, type, array_type, length)                           \
  reallocate(pointer, sizeof(type) + sizeof(array_type) * length, 0)

void *reallocate(void *previous, size_t oldSize, size_t newSize);

#endif
